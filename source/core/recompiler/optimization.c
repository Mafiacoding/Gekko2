#include "core/recompiler/optimization.h"
#include <stdio.h>
#include <string.h>
uint32_t gekko2_optimization_mask=GEKKO2_OPT_DEFAULT;
static uint32_t requested=GEKKO2_OPT_DEFAULT;
static const char *names[GEKKO2_OPT_COUNT]={
 "EE dynarec","IOP dynarec","VU0 / VU1 dynarec","EE native blocks",
 "Native block links","GPR residency","6 MiB code arena",
 "GX resident depth / blend","GX texture reuse","Fastmem RAM page cache",
 "GX independent VRAM writes","Native load / store reduction","GX Gouraud shading","Block cache reuse","HLE RAM bulk operations","Strict BIOS HLE fallback","ARM IPU worker (requires IOS service)","EE compact cache (1024 owners)","ARM startup IOS","EE cached block prefixes","CPU inactive service guards","IOP alarm / delay deadlines","Light runtime diagnostics","ARM BIOS RAM jobs (experimental)","EE native event windows (beta)","EE / IOP event batches (beta)","IOP verified idle batches (beta)","EE long blocks (32 instructions)","IOP registered service HLE (beta)","IOP catch-up native blocks (beta)","IOP adaptive block cache (beta)","SPU2 sound output (beta)"};
static const char *descriptions[GEKKO2_OPT_COUNT]={
 "Supported EE instructions use PPC; others use interpreter.",
 "Supported IOP instructions use PPC; others use interpreter.",
 "Supported VU instructions use PPC; others use interpreter.",
 "Bounded EE blocks; scheduler option controls interleave granularity.",
 "Reuse guarded native successors inside each EE grant.",
 "Keep repeated guest GPR words in preserved PPC registers.",
 "Share a fixed code pool; keep generated addresses stable.",
 "Keep supported RGB targets in GX; exact GS alpha / Z rules.",
 "Reuse decoded textures when source and state keys match.",
#ifdef GEKKO2_DSI_FASTMEM
 "EE RAM cache + DSI KSEG LW pilot; code arena ON, Cold Boot required.",
#else
 "Experimental: EE RAM TLB cache + direct IOP RAM. No PPC MMU.",
#endif
 "Skip color readback for writes outside the active target.",
 "Reduce redundant guest GPR accesses inside native bodies.",
 "Use GX for eligible triangles; retain exact-state fallbacks.",
 "Four cache ways; IOP emits only the granted instruction budget.",
 "Known BIOS memcpy / memset use bounded RAM; MMIO keeps helpers.",
 "Unknown A0/B0/C0 calls execute guest code; experimental compatibility.",
 "Experimental CSC worker. ARM / IOS changes require app restart.",
 "Reuse ON: 1024 EE owners instead of 16384; compare on cold boot.",
 "Before first boot or app restart: installed IOS222 / MLOAD required.",
 "Use existing long EE blocks for short grants; keep every boundary.",
 "Bounded host batches, VU native re-entry and inactive service guards.",
 "Scan THREADMAN only at its next deadline; retain alarm ordering.",
 "Keep session / faults / timing logs; skip historical per-PC probes.",
 "MLOAD copy / fill for large IOP BIOS RAM helpers; verified CPU fallback.",
 "Pure integer PPC batches until the next event; preserves 8:1 schedule.",
 "Up to 64 EE slots, then IOP catch-up; shared MMIO forces synchronization.",
 "Skip parked IOP CPU work strictly before peripheral / thread deadlines.",
 "Compile up to 32 instructions; guarded links and short prefixes retained.",
 "Registered SIO2 / sysclib RAM services; unsafe or changed code falls back.",
 "Run owed IOP ticks in native blocks; per-instruction IRQ and delay guards.",
 "Grow short cached IOP blocks up to 8 slots as grants increase; no duplicates.",
 "48 kHz guest-timed ADPCM + Wii DMA output. Cold Boot; slow emulation has gaps."};
uint32_t gekko2_opt_available(void)
{
 uint32_t mask=GEKKO2_OPT_VALID_MASK;
#if !defined(GEKKO) || defined(PCSX2WII_JIT_DISABLE)
 mask &= ~(GEKKO2_OPT_BIT(GEKKO2_OPT_EE_JIT)|GEKKO2_OPT_BIT(GEKKO2_OPT_IOP_JIT)|GEKKO2_OPT_BIT(GEKKO2_OPT_VU_JIT)|GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS)|GEKKO2_OPT_BIT(GEKKO2_OPT_NATIVE_LINKS)|GEKKO2_OPT_BIT(GEKKO2_OPT_RESIDENCY)|GEKKO2_OPT_BIT(GEKKO2_OPT_WORD_ALLOCATION)|GEKKO2_OPT_BIT(GEKKO2_OPT_EE_PREFIX));
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
void gekko2_opt_apply(void)
{
 gekko2_optimization_mask=gekko2_opt_requested();
}
void gekko2_opt_reset_requested(void){requested=GEKKO2_OPT_DEFAULT;}
static uint32_t profile_mask(unsigned profile)
{
 uint32_t mask=GEKKO2_OPT_DEFAULT;
 if(profile==GEKKO2_PROFILE_INTERPRETER)
  mask&=~(GEKKO2_OPT_BIT(GEKKO2_OPT_EE_JIT)|GEKKO2_OPT_BIT(GEKKO2_OPT_IOP_JIT)|
   GEKKO2_OPT_BIT(GEKKO2_OPT_VU_JIT)|GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS)|
   GEKKO2_OPT_BIT(GEKKO2_OPT_NATIVE_LINKS)|GEKKO2_OPT_BIT(GEKKO2_OPT_RESIDENCY)|
   GEKKO2_OPT_BIT(GEKKO2_OPT_WORD_ALLOCATION)|GEKKO2_OPT_BIT(GEKKO2_OPT_EE_PREFIX));
 return mask&gekko2_opt_available();
}
int gekko2_opt_request_profile(unsigned profile)
{if(profile>GEKKO2_PROFILE_INTERPRETER)return 0;requested=profile_mask(profile);return 1;}
const char *gekko2_opt_profile_name(void)
{
 uint32_t mask=gekko2_opt_requested();
 return mask==profile_mask(GEKKO2_PROFILE_BASE)?"WII BASE":
        mask==profile_mask(GEKKO2_PROFILE_INTERPRETER)?"INTERPRETER REFERENCE":"CUSTOM";
}
const char *gekko2_opt_dependency(unsigned n,uint32_t mask)
{
 if(n>=GEKKO2_OPT_COUNT)return "Invalid option";
 if(!(gekko2_opt_available()&GEKKO2_OPT_BIT(n)))return "Backend not compiled";
 if(n==GEKKO2_OPT_EE_BLOCKS && !(mask&GEKKO2_OPT_BIT(GEKKO2_OPT_EE_JIT)))return "Needs EE dynarec";
 if((n==GEKKO2_OPT_NATIVE_LINKS||n==GEKKO2_OPT_EE_PREFIX||n==GEKKO2_OPT_LONG_EE_BLOCKS||n==GEKKO2_OPT_NATIVE_EVENTS||n==GEKKO2_OPT_EVENT_SCHEDULER)&&
   (mask&(GEKKO2_OPT_BIT(GEKKO2_OPT_EE_JIT)|GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS)))!=(GEKKO2_OPT_BIT(GEKKO2_OPT_EE_JIT)|GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS)))return "Needs EE dynarec + native blocks";
 if(n==GEKKO2_OPT_NATIVE_EVENTS&&!(mask&GEKKO2_OPT_BIT(GEKKO2_OPT_EE_SERVICES)))return "Needs CPU service guards";
 if(n==GEKKO2_OPT_COMPACT_CACHE&&!(mask&GEKKO2_OPT_BIT(GEKKO2_OPT_CACHE_REUSE)))return "Needs block cache reuse";
 if((n==GEKKO2_OPT_IOP_CATCHUP_BLOCKS||n==GEKKO2_OPT_IOP_BLOCK_GROWTH)&&!(mask&GEKKO2_OPT_BIT(GEKKO2_OPT_IOP_JIT)))return "Needs IOP dynarec";
 if(n==GEKKO2_OPT_IOP_CATCHUP_BLOCKS&&!(mask&GEKKO2_OPT_BIT(GEKKO2_OPT_EVENT_SCHEDULER)))return "Needs EE / IOP event batches";
 if(n==GEKKO2_OPT_ARM_IOS222&&!(mask&(GEKKO2_OPT_BIT(GEKKO2_OPT_ARM_WORKER)|GEKKO2_OPT_BIT(GEKKO2_OPT_ARM_CPU_RAM))))return "Needs an ARM job option; app restart";
 if(n==GEKKO2_OPT_ARM_WORKER||n==GEKKO2_OPT_ARM_CPU_RAM)return "Requires runtime IOS service; CPU fallback";
 return "";
}
int gekko2_opt_load(const char *path)
{
 FILE *f=fopen(path,"r");
 /* libfat may require moving the old destination before rename. A backup
  * also recovers a power loss between those two renames. */
 if(!f){char backup[256];if(snprintf(backup,sizeof backup,"%s.bak",path)>=(int)sizeof backup)return -1;f=fopen(backup,"r");}
 if(!f)return -1;
 unsigned version=0,mask=0;char extra;
 int n=fscanf(f,"GEKKO2_OPTIONS %u %x %c",&version,&mask,&extra);fclose(f);
 if(n!=2||(version<1u||version>9u)||mask&~(GEKKO2_OPT_VALID_MASK)||(version<9u&&(mask&GEKKO2_OPT_BIT(GEKKO2_OPT_SOUND)))||(version<8u&&(mask&(GEKKO2_OPT_BIT(GEKKO2_OPT_IOP_CATCHUP_BLOCKS)|GEKKO2_OPT_BIT(GEKKO2_OPT_IOP_BLOCK_GROWTH))))||(version<7u&&(mask&(GEKKO2_OPT_BIT(GEKKO2_OPT_EVENT_SCHEDULER)|GEKKO2_OPT_BIT(GEKKO2_OPT_IOP_IDLE_BATCH)|GEKKO2_OPT_BIT(GEKKO2_OPT_LONG_EE_BLOCKS)|GEKKO2_OPT_BIT(GEKKO2_OPT_IOP_SERVICE_HLE))))||(version==1u&&mask>0x1fffu)||(version<4u&&(mask&GEKKO2_OPT_BIT(GEKKO2_OPT_ARM_IOS222)))||(version<6u&&(mask&GEKKO2_OPT_BIT(GEKKO2_OPT_NATIVE_EVENTS)))||(version<5u&&(mask&(GEKKO2_OPT_R1342_DEFAULT|GEKKO2_OPT_BIT(GEKKO2_OPT_ARM_CPU_RAM)))))return -1;
 if(version==1u)mask|=GEKKO2_OPT_CACHE_DEFAULT;
 if(version<3u)mask|=GEKKO2_OPT_BIT(GEKKO2_OPT_HLE_RAM);
 if(version<5u)mask|=GEKKO2_OPT_R1342_DEFAULT;
 requested=mask&gekko2_opt_available();return 0;
}
int gekko2_opt_save(const char *path)
{
 char temp[256],backup[256];
 if(snprintf(temp,sizeof(temp),"%s.tmp",path)>=(int)sizeof(temp)||snprintf(backup,sizeof backup,"%s.bak",path)>=(int)sizeof backup)return -1;
 FILE *f=fopen(temp,"w");if(!f)return -1;
 int ok=fprintf(f,"GEKKO2_OPTIONS 9 %08lx\n",(unsigned long)gekko2_opt_requested())>0;
 if(fclose(f))ok=0;
 if(!ok){remove(temp);return -1;}
 if(!rename(temp,path)){remove(backup);return 0;}
 /* FAT rename implementations can reject an existing destination. Keep the
  * previous complete file until replacement succeeds; roll back on failure. */
 remove(backup);
 if(rename(path,backup)){remove(temp);return -1;}
 if(rename(temp,path)){rename(backup,path);remove(temp);return -1;}
 remove(backup);return 0;
}
