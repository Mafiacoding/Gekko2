/* GPL-3.0-or-later.
 * PPC HTAB and tuxedo exception convention adapted from BenoitAdam/nullDC4Wii
 * wii/wii_fastmem.cpp, commit 26a623e3feca9927d8ffe43706b06d82d672dd64.
 * Guest mappings/emitter protocol are Gekko2-specific. Only EE KSEG LW uses
 * this window. No guest TLB, stores, VRAM or MMIO is mapped into the HTAB.
 */
#include "core/recompiler/dsi_fastmem.h"
#include "core/recompiler/ppc_code_cache.h"
#include "core/recompiler/optimization.h"
#if defined(GEKKO) && defined(GEKKO2_DSI_FASTMEM)
#include <gccore.h>
#include <tuxedo/ppc/exception.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#define HTAB_BYTES 0x40000u
#define VSID 0x47520u
static uint32_t *htab;
static uint32_t previous_sdr1,previous_sr[2];
static PPCExcptHandlerFn previous_handler;
int g_gekko2_dsi_active;
uint32_t g_gekko2_dsi_block_prepares;
static int status,installed;
static uint32_t counters[7]; /* faults, patches, PC, DAR, probes, sites, block sites */
extern uint32_t __ppc_excpt_buf[];
extern void gekko2_dsi_entry(void);
extern uint32_t gekko2_dsi_probe_word(uint32_t address);
static volatile int probe,probe_fault;
static void restore(void);
static void invalidate_tlb(void)
{
 for(uint32_t i=0;i<64;i++)__asm__ volatile("tlbie %0"::"r"(i<<12):"memory");
 __asm__ volatile("sync; isync":::"memory");
}
void dsi_fastmem_reserve(void)
{
 restore();g_gekko2_dsi_active=0;
 status=0;memset(counters,0,sizeof(counters));
 g_gekko2_dsi_block_prepares=0;
 if(!gekko2_opt_enabled(GEKKO2_OPT_FASTMEM))return;
 if(!gekko2_opt_enabled(GEKKO2_OPT_CODE_ARENA)){status=-7;return;}
 if(!htab)htab=memalign(HTAB_BYTES,HTAB_BYTES);
 if(!htab)status=-1;
}
static int map_page(uint32_t ea,uint32_t pa)
{
 uint32_t vsid=VSID+(ea>>28)-2u;
 uint32_t hash=(vsid&0x7ffffu)^((ea>>12)&0xffffu);
 uint32_t w0=0x80000000u|(vsid<<7)|((ea>>22)&63u);
 uint32_t w1=(pa&0xfffff000u)|0x180u|3u; /* read-only alias */
 for(unsigned secondary=0;secondary<2;secondary++){
  uint32_t *group=htab+((hash&0xfffu)*16u);
  for(unsigned slot=0;slot<8;slot++)if(!(group[slot*2]&0x80000000u)){
   group[slot*2+1]=w1;group[slot*2]=w0|(secondary<<6);return 1;
  }
  hash=~hash;
 }
 return 0;
}
static void restore(void)
{
 if(!installed)return;
 uint32_t irq=IRQ_Disable();
 g_gekko2_dsi_active=0;
 __asm__ volatile("mtspr 25,%0"::"r"(previous_sdr1):"memory");
 for(unsigned i=0;i<2;i++)__asm__ volatile("mtsrin %0,%1"::"r"(previous_sr[i]),"r"((i+2u)<<28):"memory");
 invalidate_tlb();PPCExcptSetHandler(PPC_EXCPT_DSI,previous_handler);
 installed=0;IRQ_Restore(irq);
}
void dsi_fastmem_shutdown(void)
{
 restore();if(htab){free(htab);htab=NULL;}
}
void dsi_fastmem_init(void *ram,uint32_t bytes)
{
 if(!htab||status<0||!gekko2_opt_enabled(GEKKO2_OPT_FASTMEM))return;
 if(!ram||((uintptr_t)ram&4095u)||bytes!=0x02000000u){status=-2;return;}
 uint32_t msr;
 __asm__ volatile("mfmsr %0":"=r"(msr));
 if(!(msr&0x10u)){status=-8;return;}
 __asm__ volatile("mfspr %0,25":"=r"(previous_sdr1));
 previous_handler=PPCExcptGetHandler(PPC_EXCPT_DSI);
 /* Replacing somebody else's page table/handler would break their mappings. */
 if(previous_sdr1||(previous_handler&&previous_handler!=PPCExcptDefaultHandler)){status=-3;return;}
 for(unsigned i=0;i<2;i++)__asm__ volatile("mfsrin %0,%1":"=r"(previous_sr[i]):"r"((i+2u)<<28));
 memset(htab,0,HTAB_BYTES);
 uint32_t physical=MEM_VIRTUAL_TO_PHYSICAL(ram);
 for(uint32_t offset=0;offset<bytes;offset+=4096u)
  if(!map_page(GEKKO2_DSI_WINDOW+offset,physical+offset)){status=-4;return;}
 DCFlushRange(htab,HTAB_BYTES);
 uint32_t irq=IRQ_Disable();
 PPCExcptSetHandler(PPC_EXCPT_DSI,gekko2_dsi_entry);installed=1;
 __asm__ volatile("mtspr 25,%0"::"r"(MEM_VIRTUAL_TO_PHYSICAL(htab)|3u):"memory");
 for(unsigned i=0;i<2;i++)__asm__ volatile("mtsrin %0,%1"::"r"(VSID+i),"r"((i+2u)<<28):"memory");
 invalidate_tlb();IRQ_Restore(irq);
 /* Read-only aliases at both ends. Distinct sentinels prevent zero-filled
  * unrelated memory from falsely passing; restore every host word. */
 const uint32_t offsets[]={0u,4096u,0x01fffffcu};
 for(unsigned i=0;i<3;i++){
  uint32_t *host=(uint32_t *)((uint8_t *)ram+offsets[i]);
  uint32_t saved=*host,expected=0x474b5100u+i;
  *host=expected;DCFlushRange((void *)((uintptr_t)host&~31u),32);
  probe=1;probe_fault=0;counters[4]++;
  uint32_t got=gekko2_dsi_probe_word(GEKKO2_DSI_WINDOW+offsets[i]);
  probe=0;
  *host=saved;DCFlushRange((void *)((uintptr_t)host&~31u),32);
  if(probe_fault||got!=expected){status=-5;restore();return;}
 }
 /* Prove unmapped MMIO faults through our handler before generating sites. */
 probe=1;probe_fault=0;counters[4]++;
 (void)gekko2_dsi_probe_word(GEKKO2_DSI_WINDOW+0x10000000u);probe=0;
 if(!probe_fault){status=-6;restore();return;}
 status=1;g_gekko2_dsi_active=1;
}
int dsi_fastmem_enabled(void)
{
 return g_gekko2_dsi_active&&gekko2_opt_enabled(GEKKO2_OPT_FASTMEM)&&gekko2_opt_enabled(GEKKO2_OPT_CODE_ARENA);
}
int dsi_fastmem_status(void){return status;}
uint32_t dsi_fastmem_stat(unsigned n){return n==7?g_gekko2_dsi_block_prepares:(n<7?counters[n]:0);}
void dsi_fastmem_note_site(void){counters[5]++;}
void dsi_fastmem_note_block_site(void){counters[5]++;counters[6]++;}
/* Exception path: no FP, allocation, stdio, IPC, scheduler or memory helpers.
 * rfi retries a branch to the original in-owner C fallback in normal context.
 * Metadata is relative so arena publication never leaves stale pointers.
 */
int gekko2_dsi_handle(uint32_t pc)
{
 if(probe&&pc==(uint32_t)(uintptr_t)gekko2_dsi_probe_word){
  probe_fault=1;__ppc_excpt_buf[0]=pc+4u;return 0;
 }
 uintptr_t lo,hi;
 if(!g_gekko2_dsi_active||!ppc_code_cache_live_bounds((void *)(uintptr_t)pc,&lo,&hi)||pc<lo+12u||pc+4u>hi)return -1;
 volatile uint32_t *site=(volatile uint32_t *)(uintptr_t)pc;
 /* b+12, magic, relative fallback, lwbrx r7,0,r5 */
 if(site[-3]!=0x4800000cu||site[-2]!=GEKKO2_DSI_MARKER||site[0]!=0x7ce02c2cu||(site[-1]&0xfc000003u)!=0x48000000u)return -1;
 int32_t delta=(int32_t)((site[-1]&0x03fffffcu)<<6)>>6;
 uintptr_t target=(uintptr_t)pc+delta;
 if(target<pc+4u||target>=hi)return -1;
 uint32_t dar,dsisr;
 __asm__ volatile("mfspr %0,19":"=r"(dar));
 __asm__ volatile("mfspr %0,18":"=r"(dsisr));
 if(dar<GEKKO2_DSI_WINDOW||dar>=GEKKO2_DSI_WINDOW+0x20000000u||!(dsisr&0x40000000u)||(dsisr&0x02000000u))return -1;
 counters[0]++;counters[2]=pc;counters[3]=dar;
 site[0]=site[-1];
 /* Single-word publication, integer-only cache operations. */
 __asm__ volatile("dcbst 0,%0; sync; icbi 0,%0; sync; isync"::"r"(pc):"memory");
 counters[1]++;return 0;
}
#else
void dsi_fastmem_reserve(void){}
void dsi_fastmem_init(void *ram,uint32_t bytes){(void)ram;(void)bytes;}
void dsi_fastmem_shutdown(void){}
int dsi_fastmem_enabled(void){return 0;}
int dsi_fastmem_status(void){return 0;}
uint32_t dsi_fastmem_stat(unsigned n){(void)n;return 0;}
void dsi_fastmem_note_site(void){}
void dsi_fastmem_note_block_site(void){}
int gekko2_dsi_handle(uint32_t pc){(void)pc;return -1;}
#endif
