/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPU2 native register/FIFO/DMA/timing beta. Architectural contracts:
 * PCSX2 SPU2/{RegTable,spu2sys,Dma,ReadInput,Mixer}.cpp;
 * ps2sdk common/include/libsd-common.h. Normal DMA completion remains
 * synchronous; AutoDMA completion follows consumption of split PCM.
 */
#include "core/hw/iop_spu2.h"
#include "core/hw/spu2_mixer.h"
#include <string.h>
#include "core/hw/iop_dma.h"
#include "core/hw/iop_intc.h"
#include "core/hw/audio_output.h"
#include "core/iop/iop_core.h"
#include "core/recompiler/optimization.h"
#ifndef GEKKO
extern iop_state_t *iop_core_get_state(void) __attribute__((weak));
#endif
static uint64_t audio_clock;
static uint32_t audio_stats[4];
static unsigned audio_busy;
uint32_t g_spu2_irq_clock_required,g_spu2_next_sample;
typedef struct {uint32_t active,madr,left,pos,valid[2];} adma_state;
static adma_state adma[2];


static uint8_t g_regs[IOP_SPU2_SIZE];

void iop_spu2_init(void)
{
    memset(g_regs, 0, sizeof(g_regs));
    spu2_mixer_init();audio_clock=0;audio_busy=0;g_spu2_next_sample=768u;g_spu2_irq_clock_required=0;
    memset(audio_stats,0,sizeof audio_stats);memset(adma,0,sizeof adma);
    audio_output_reset();
}

uint32_t iop_spu2_voice_reg_addr(int core, int voice, uint32_t voice_reg_offset)
{
    uint32_t core_base = IOP_SPU2_BASE + (core ? SPU2_CORE1_OFFSET : 0u);
    return core_base + (uint32_t)voice * SPU2_VOICE_STRIDE + voice_reg_offset;
}

uint32_t iop_spu2_voice_addr_reg_addr(int core, int voice, uint32_t vaddr_reg_offset)
{
    uint32_t core_base = IOP_SPU2_BASE + (core ? SPU2_CORE1_OFFSET : 0u);
    return core_base + SPU2_VADDR_BASE + (uint32_t)voice * SPU2_VADDR_STRIDE + vaddr_reg_offset;
}

uint32_t iop_spu2_core_reg_addr(int core, uint32_t core_reg_offset)
{
    uint32_t core_base = IOP_SPU2_BASE + (core ? SPU2_CORE1_OFFSET : 0u);
    return core_base + core_reg_offset;
}

/* Raw 16-bit storage accessors, no side effects - used internally by
 * both the plain passthrough path and the side-effect logic below so
 * there is exactly one place that touches g_regs directly. */
static uint16_t raw_read16(uint32_t off)
{
    return (uint16_t)g_regs[off] | ((uint16_t)g_regs[off + 1] << 8);
}

static void raw_write16(uint32_t off, uint16_t value)
{
    g_regs[off]     = (uint8_t)(value & 0xFFu);
    g_regs[off + 1] = (uint8_t)((value >> 8) & 0xFFu);
}

void iop_spu2_set_raw16(uint32_t addr,uint16_t value)
{
 if(addr>=IOP_SPU2_BASE&&addr<=IOP_SPU2_BASE+IOP_SPU2_SIZE-2u&&!(addr&1u))raw_write16(addr-IOP_SPU2_BASE,value);
}
static uint32_t core_address(int core,uint32_t hi)
{return (((uint32_t)raw_read16((core?0x400u:0u)+hi)<<16)|raw_read16((core?0x400u:0u)+hi+2u))&0xfffffu;}
static void set_address(int core,uint32_t hi,uint32_t word)
{
 uint32_t off=(core?0x400u:0u)+hi;raw_write16(off,(word>>16)&15u);raw_write16(off+2u,(uint16_t)word);
}
void iop_spu2_check_ram_irq(uint32_t first,uint32_t words)
{
 first&=0xfffffu;
 for(int core=0;core<2;core++)if(raw_read16((core?0x400u:0u)+SPU2_C_CTRL)&0x40u){
  uint32_t irq=core_address(core,SPU2_C_IRQA_HI);
  if(((irq-first)&0xfffffu)<words){
   uint16_t flags=raw_read16(SPU2_S_IRQINFO),bit=(uint16_t)(4u<<core);
   if(!(flags&bit)){raw_write16(SPU2_S_IRQINFO,flags|bit);iop_intc_raise(9);audio_stats[1]++;}
  }
 }
}
void iop_spu2_sync_now(void)
{
 if(audio_busy)return;
#ifndef GEKKO
 if(!iop_core_get_state)return;
#endif
 iop_spu2_sync(iop_core_get_state()->sched_ticks);
}
void iop_spu2_sync(uint64_t ticks)
{
 if(audio_busy)return;
 if(ticks<audio_clock){audio_clock=ticks;return;}
 uint64_t frames=(ticks-audio_clock)/768u; /* 36.864 MHz / 48 kHz */
 if(!frames)return;
 audio_busy=1;audio_clock+=frames*768u;g_spu2_next_sample=(uint32_t)audio_clock+768u;
 int16_t pcm[64*2];
 while(frames){unsigned n=frames>64u?64u:(unsigned)frames;
  spu2_mixer_render_frames(pcm,(int)n);
  if(gekko2_opt_enabled(GEKKO2_OPT_SOUND))audio_output_submit(pcm,n);
  frames-=n;audio_stats[0]+=n;
 }
 audio_busy=0;
}
unsigned iop_spu2_event_distance(unsigned maximum)
{
 /* An armed SPU IRQ may arise at the next sample; keep parked IOP
  * batches strictly before it. Non-audio code never wakes a host thread. */
 if((raw_read16(SPU2_C_CTRL)|raw_read16(0x400u+SPU2_C_CTRL))&0x40u){
#ifndef GEKKO
  if(!iop_core_get_state)return maximum;
#endif
  uint64_t now=iop_core_get_state()->sched_ticks;
  unsigned distance=now>=audio_clock?(unsigned)(768u-(now-audio_clock)%768u):1u;
  if(now>=audio_clock+768u)distance=1u;
  if(distance<maximum)maximum=distance;
 }
 return maximum;
}
uint32_t iop_spu2_audio_stat(unsigned index){return index<4u?audio_stats[index]:0;}
static int adma_fill(int core,unsigned half)
{
 adma_state *a=&adma[core];if(!a->left){a->valid[half]=0;return 1;}
#ifndef GEKKO
 if(!iop_core_get_state)return 0;
#endif
 iop_state_t *iop=iop_core_get_state();
 if(!iop->ram||a->left<1024u||a->madr>iop->ram_size||1024u>iop->ram_size-a->madr)return 0;
 uint32_t word=0x2000u+(unsigned)core*0x400u+half*0x100u;
 uint8_t *ram=spu2_mixer_get_ram();
 memcpy(ram+word*2u,iop->ram+a->madr,512u);
 memcpy(ram+(word+0x200u)*2u,iop->ram+a->madr+512u,512u);
 iop_spu2_check_ram_irq(word,256u);iop_spu2_check_ram_irq(word+0x200u,256u);
 a->madr+=1024u;a->left-=1024u;a->valid[half]=256u;return 1;
}
int iop_spu2_adma_begin(int core,uint32_t madr,uint32_t bytes)
{
 if(core<0||core>1)return 0;
 if(!(raw_read16((core?0x400u:0u)+SPU2_C_ADMAS)&(1u<<core)))return 0;
 /* Normal 16-bit split stereo only. Reject partial blocks explicitly;
  * AC3 bypass/CDDA32 are not silently treated as this format. */
 if(!bytes||(bytes&1023u)||(raw_read16(0x7c0u)&0x104u)){audio_stats[3]++;return -1;}
#ifndef GEKKO
 if(!iop_core_get_state)return -1;
#endif
 iop_state_t *iop=iop_core_get_state();
 if(!iop->ram||madr>iop->ram_size||bytes>iop->ram_size-madr){audio_stats[3]++;return -1;}
 adma[core]=(adma_state){1,madr,bytes,0,{0,0}};
 if(!adma_fill(core,0)||!adma_fill(core,1)){memset(&adma[core],0,sizeof adma[core]);return -1;}
 uint32_t status=(core?0x400u:0u)+SPU2_C_STATX;raw_write16(status,(raw_read16(status)&~0x80u)|0x400u);
 audio_stats[2]++;return 1;
}
void iop_spu2_adma_cancel(int core){if(core>=0&&core<2){memset(&adma[core],0,sizeof adma[core]);
 uint32_t status=(core?0x400u:0u)+SPU2_C_STATX;raw_write16(status,raw_read16(status)&~0x400u);}}
void iop_spu2_adma_sample(int core,int16_t out[2])
{
 out[0]=out[1]=0;adma_state *a=&adma[core];if(!a->active)return;
 unsigned half=a->pos>>8,index=a->pos&255u;
 if(a->valid[half]){
  uint32_t word=0x2000u+(unsigned)core*0x400u+a->pos;
  uint8_t *ram=spu2_mixer_get_ram();
  iop_spu2_check_ram_irq(word,1);iop_spu2_check_ram_irq(word+0x200u,1);
  out[0]=(int16_t)((uint16_t)ram[word*2u]|((uint16_t)ram[word*2u+1u]<<8));
  word+=0x200u;out[1]=(int16_t)((uint16_t)ram[word*2u]|((uint16_t)ram[word*2u+1u]<<8));
  a->valid[half]--;
 }
 a->pos=(a->pos+1u)&511u;
 if(index==255u){
  iop_dma_spu2_progress(core,1024u,!a->left&&!a->valid[0]&&!a->valid[1]);
  if(!a->left&&!a->valid[0]&&!a->valid[1])iop_spu2_adma_cancel(core);
  else if(!adma_fill(core,half)){audio_stats[3]++;a->active=0;}
 }
}

/* Round 523/524: real KON-clears-ENDX and CTRL[5:0]->STATX[5:0]
 * side effects. addr is an absolute SPU2 address (already known to
 * be in-range by the caller); value is the 16-bit value just written
 * there by the plain passthrough path. */
static void apply_write_side_effects(uint32_t addr, uint16_t value)
{
    int core;
    for (core = 0; core < 2; core++) {
        uint32_t kon0_addr  = iop_spu2_core_reg_addr(core, SPU2_C_KON0);
        uint32_t kon1_addr  = iop_spu2_core_reg_addr(core, SPU2_C_KON1);
        uint32_t koff0_addr = iop_spu2_core_reg_addr(core, SPU2_C_KOFF0);
        uint32_t koff1_addr = iop_spu2_core_reg_addr(core, SPU2_C_KOFF1);
        uint32_t ctrl_addr  = iop_spu2_core_reg_addr(core, SPU2_C_CTRL);
        uint32_t endx0_off  = iop_spu2_core_reg_addr(core, SPU2_C_ENDX0) - IOP_SPU2_BASE;
        uint32_t endx1_off  = iop_spu2_core_reg_addr(core, SPU2_C_ENDX1) - IOP_SPU2_BASE;
        uint32_t statx_off  = iop_spu2_core_reg_addr(core, SPU2_C_STATX) - IOP_SPU2_BASE;

        if (addr == kon0_addr) {
            /* Real hardware: "The bits get CLEARED when setting the
             * corresponding KEY ON bits." KON0 covers voices 0-15,
             * same bit layout as ENDX0. */
            uint16_t endx = raw_read16(endx0_off);
            raw_write16(endx0_off, (uint16_t)(endx & ~value));
            /* Round 711: real KON also starts the actual ADSR/ADPCM
             * synthesis engine (spu2_mixer.c) for every bit set here -
             * see that file for the full real KON semantics citation. */
            spu2_mixer_on_kon(core, 0, value);
        } else if (addr == kon1_addr) {
            /* KON1 covers voices 16-23, same bit layout as ENDX1. */
            uint16_t endx = raw_read16(endx1_off);
            raw_write16(endx1_off, (uint16_t)(endx & ~value));
            spu2_mixer_on_kon(core, 1, value);
        } else if (addr == koff0_addr) {
            /* Round 711: real KOFF starts Release on every bit set here
             * (voices 0-15) - see spu2_mixer.c. */
            spu2_mixer_on_koff(core, 0, value);
        } else if (addr == koff1_addr) {
            spu2_mixer_on_koff(core, 1, value); /* voices 16-23 */
        } else if (addr == ctrl_addr) {
            /* Real hardware: SPUSTAT bits 5-0 mirror SPUCNT bits 5-0
             * ("applied a bit delayed" on real hardware; this project
             * has no cycle-accurate timing model, so the mirror is
             * applied immediately - documented simplification). */
            g_spu2_irq_clock_required=!!((raw_read16(SPU2_C_CTRL)|raw_read16(0x400u+SPU2_C_CTRL))&0x40u);
            if(!(value&0x40u))raw_write16(SPU2_S_IRQINFO,raw_read16(SPU2_S_IRQINFO)&~(4u<<core));
            uint16_t statx = raw_read16(statx_off);
            statx = (uint16_t)((statx & ~0x3Fu) | (value & 0x3Fu));
            raw_write16(statx_off, statx);
        }
    }
}

int iop_spu2_mmio_read16(uint32_t addr, uint16_t *out)
{
    if(!out)return 0;
    if (addr < IOP_SPU2_BASE || addr > IOP_SPU2_BASE + IOP_SPU2_SIZE - 2u || (addr&1u)) return 0;
    iop_spu2_sync_now();
    uint32_t off = addr - IOP_SPU2_BASE;
    unsigned core=(off>=0x400u&&off<0x760u)?1u:0u;
    if(off==(core?0x400u:0u)+SPU2_C_TDATA){
      uint32_t word=core_address(core,SPU2_C_TSA_HI);uint8_t *ram=spu2_mixer_get_ram();
      iop_spu2_check_ram_irq(word,1);*out=(uint16_t)ram[word*2u]|((uint16_t)ram[word*2u+1u]<<8);
      set_address(core,SPU2_C_TSA_HI,(word+1u)&0xfffffu);
    }else *out = raw_read16(off);
    return 1;
}

int iop_spu2_mmio_write16(uint32_t addr, uint16_t value)
{
    if (addr < IOP_SPU2_BASE || addr > IOP_SPU2_BASE + IOP_SPU2_SIZE - 2u || (addr&1u)) return 0;
    iop_spu2_sync_now();
    uint32_t off = addr - IOP_SPU2_BASE;
    unsigned core=(off>=0x400u&&off<0x760u)?1u:0u;
    if(off==(core?0x400u:0u)+SPU2_C_TDATA){
      uint32_t word=core_address(core,SPU2_C_TSA_HI);uint8_t *ram=spu2_mixer_get_ram();
      iop_spu2_check_ram_irq(word,1);ram[word*2u]=(uint8_t)value;ram[word*2u+1u]=(uint8_t)(value>>8);
      set_address(core,SPU2_C_TSA_HI,(word+1u)&0xfffffu);
    }
    raw_write16(off, value);
    uint32_t local=off-(core?0x400u:0u);
    if(local>=SPU2_VADDR_BASE&&local<SPU2_VADDR_BASE+24u*SPU2_VADDR_STRIDE){
      unsigned lane=(local-SPU2_VADDR_BASE)%SPU2_VADDR_STRIDE;
      if(lane==SPU2_VA_LSA_HI||lane==SPU2_VA_LSA_LO)spu2_mixer_on_loop_address_write(core,(local-SPU2_VADDR_BASE)/SPU2_VADDR_STRIDE);
    }
    apply_write_side_effects(addr, value);
    return 1;
}

int iop_spu2_mmio_read32(uint32_t addr,uint32_t *out)
{
 if(!out||addr<IOP_SPU2_BASE||addr>IOP_SPU2_BASE+IOP_SPU2_SIZE-4u||(addr&1u))return 0;
 uint16_t lo,hi;iop_spu2_mmio_read16(addr,&lo);iop_spu2_mmio_read16(addr+2u,&hi);*out=lo|((uint32_t)hi<<16);return 1;
}
int iop_spu2_mmio_write32(uint32_t addr,uint32_t value)
{
 if(addr<IOP_SPU2_BASE||addr>IOP_SPU2_BASE+IOP_SPU2_SIZE-4u||(addr&1u))return 0;
 iop_spu2_mmio_write16(addr,(uint16_t)value);iop_spu2_mmio_write16(addr+2u,(uint16_t)(value>>16));return 1;
}

/* Pointer-free optional checkpoint. Host DMA pages and queue are excluded. */
typedef struct {uint32_t version,mixer_size;uint64_t clock;uint32_t stats[4];
 uint8_t regs[IOP_SPU2_SIZE];adma_state stream[2];uint8_t mixer[8192];} spu_snapshot;
uint32_t iop_spu2_snapshot_size(void){return sizeof(spu_snapshot);}
void iop_spu2_snapshot_save(void *out)
{
 iop_spu2_sync_now();spu_snapshot *s=out;memset(s,0,sizeof *s);s->version=1;s->mixer_size=spu2_mixer_snapshot_size();
 s->clock=audio_clock;memcpy(s->stats,audio_stats,sizeof audio_stats);memcpy(s->regs,g_regs,sizeof g_regs);
 memcpy(s->stream,adma,sizeof adma);spu2_mixer_snapshot_save(s->mixer);
}
int iop_spu2_snapshot_valid(const void *data,uint64_t ticks,uint32_t ram_size)
{
 const spu_snapshot *s=data;if(s->version!=1u||s->mixer_size!=spu2_mixer_snapshot_size()||
  s->mixer_size>sizeof s->mixer||s->clock>ticks||!spu2_mixer_snapshot_valid(s->mixer))return 0;
 for(int c=0;c<2;c++){const adma_state *a=&s->stream[c];if(a->active>1||a->pos>=512u||a->valid[0]>256u||a->valid[1]>256u||
  a->madr>ram_size||a->left>ram_size-a->madr||(a->left&1023u))return 0;}
 return 1;
}
void iop_spu2_snapshot_load(const void *data)
{
 const spu_snapshot *s=data;audio_output_reset();audio_clock=s->clock;audio_busy=0;memcpy(g_regs,s->regs,sizeof g_regs);
 memcpy(adma,s->stream,sizeof adma);memcpy(audio_stats,s->stats,sizeof audio_stats);spu2_mixer_snapshot_load(s->mixer);
 g_spu2_next_sample=(uint32_t)audio_clock+768u;
 g_spu2_irq_clock_required=!!((raw_read16(SPU2_C_CTRL)|raw_read16(0x400u+SPU2_C_CTRL))&0x40u);
}
void iop_spu2_restore_legacy(uint64_t ticks){iop_spu2_init();audio_clock=ticks;g_spu2_next_sample=(uint32_t)ticks+768u;}
