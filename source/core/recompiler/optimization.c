#include "core/recompiler/optimization.h"
#include <stdio.h>
#include <string.h>
uint32_t gekko2_optimization_mask=GEKKO2_OPT_DEFAULT;
static uint32_t requested=GEKKO2_OPT_DEFAULT;
static const char *names[GEKKO2_OPT_COUNT]={
 "EE dynarec","IOP dynarec","VU0 / VU1 dynarec","EE native blocks",
 "Native block links","GPR residency","6 MiB code arena",
 "GX resident depth / blend","GX texture reuse","Fastmem RAM page cache",
 "GX independent VRAM writes","Native load / store reduction","GX Gouraud shading","Block cache reuse","HLE RAM bulk operations","Strict BIOS HLE fallback","ARM IPU worker (requires IOS service)","EE compact cache (1024 owners)"};
static const char *descriptions[GEKKO2_OPT_COUNT]={
 "Supported EE instructions use PPC; others use interpreter.",
 "Supported IOP instructions use PPC; others use interpreter.",
 "Supported VU instructions use PPC; others use interpreter.",
 "Bounded EE blocks; original 8:1 EE / IOP schedule.",
 "Reuse guarded native successors inside each EE grant.",
 "Keep repeated guest GPR words in preserved PPC registers.",
 "Share a fixed code pool; keep generated addresses stable.",
 "Keep supported RGB targets in GX; exact GS alpha / Z rules.",
 "Reuse decoded textures when source and state keys match.",
 "Experimental: EE RAM TLB cache + direct IOP RAM. No PPC MMU.",
 "Skip color readback for writes outside the active target.",
 "Reduce redundant guest GPR accesses inside native bodies.",
 "Use GX for eligible triangles; retain exact-state fallbacks.",
 "Four cache ways; IOP emits only the granted instruction budget.",
 "Known BIOS memcpy / memset use bounded RAM; MMIO keeps helpers.",
 "Unknown A0/B0/C0 calls execute guest code; experimental compatibility.",
 "Experimental /dev/gekko2 CSC service; unavailable means CPU fallback.",
 "Reuse ON: 1024 EE owners instead of 4096; compare on cold boot."};
uint32_t gekko2_opt_available(void)
{
 uint32_t mask=(1u<<GEKKO2_OPT_COUNT)-1u;
#if !defined(GEKKO) || defined(PCSX2WII_JIT_DISABLE)
 mask &= ~(GEKKO2_OPT_BIT(GEKKO2_OPT_EE_JIT)|GEKKO2_OPT_BIT(GEKKO2_OPT_IOP_JIT)|GEKKO2_OPT_BIT(GEKKO2_OPT_VU_JIT)|GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS)|GEKKO2_OPT_BIT(GEKKO2_OPT_NATIVE_LINKS)|GEKKO2_OPT_BIT(GEKKO2_OPT_RESIDENCY)|GEKKO2_OPT_BIT(GEKKO2_OPT_WORD_ALLOCATION));
#endif
#if defined(GEKKO2_LEGACY_SCHEDULER) || defined(PCSX2WII_LEGACY_FRAME_REPAIR)
 mask &= ~GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS);
#endif
#ifdef GEKKO2_CODE_ARENA_DISABLE
 mask &= ~GEKKO2_OPT_BIT(GEKKO2_OPT_CODE_ARENA);
#endif
#ifdef GEKKO2_GOURAUD_GX_DISABLE
 mask &= ~GEKKO2_OPT_BIT(GEKKO2_OPT_GX_GOURAUD);
#endif
#if !defined(GEKKO) || defined(GEKKO2_GX_RESIDENT_PIPELINE_DISABLE)
 mask &= ~GEKKO2_OPT_BIT(GEKKO2_OPT_GX_RESIDENT);
#endif
 return mask;
}
uint32_t gekko2_opt_requested(void){return requested&gekko2_opt_available();}
const char *gekko2_opt_name(unsigned n){return n<GEKKO2_OPT_COUNT?names[n]:"";}
const char *gekko2_opt_description(unsigned n){return n<GEKKO2_OPT_COUNT?descriptions[n]:"";}
int gekko2_opt_toggle(unsigned n)
{
 if(n>=GEKKO2_OPT_COUNT||!(gekko2_opt_available()&GEKKO2_OPT_BIT(n)))return 0;
 requested^=GEKKO2_OPT_BIT(n);return 1;
}
void gekko2_opt_apply(void){gekko2_optimization_mask=gekko2_opt_requested();}
int gekko2_opt_load(const char *path)
{
 FILE *f=fopen(path,"r");if(!f)return -1;
 unsigned version=0,mask=0;char extra;
 int n=fscanf(f,"GEKKO2_OPTIONS %u %x %c",&version,&mask,&extra);fclose(f);
 if(n!=2||(version!=1u&&version!=2u&&version!=3u)||mask&~((1u<<GEKKO2_OPT_COUNT)-1u)||(version==1u&&mask>0x1fffu))return -1;
 if(version==1u)mask|=GEKKO2_OPT_CACHE_DEFAULT;
 if(version<3u)mask|=GEKKO2_OPT_BIT(GEKKO2_OPT_HLE_RAM);
 requested=mask&gekko2_opt_available();return 0;
}
int gekko2_opt_save(const char *path)
{
 char temp[256];if(snprintf(temp,sizeof(temp),"%s.tmp",path)>=(int)sizeof(temp))return -1;
 FILE *f=fopen(temp,"w");if(!f)return -1;
 int ok=fprintf(f,"GEKKO2_OPTIONS 3 %08lx\n",(unsigned long)gekko2_opt_requested())>0;
 if(fclose(f))ok=0;
 if(!ok||rename(temp,path)){remove(temp);return -1;}return 0;
}
