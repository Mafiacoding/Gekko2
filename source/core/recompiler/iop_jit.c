#include "core/recompiler/iop_jit.h"
#include "core/recompiler/ppc_dynarec.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
_Static_assert(offsetof(iop_state_t,gpr)==0,"IOP JIT context must start at gpr");
_Static_assert(offsetof(iop_state_t,hi)==136,"IOP HI layout");
_Static_assert(offsetof(iop_state_t,lo)==140,"IOP LO layout");
_Static_assert(offsetof(iop_state_t,cop0)==144,"IOP COP0 layout");
static uint64_t executed,rejected_hits;
static uint32_t cache_size;
static uint64_t block_runs,block_ticks;
static uint32_t block_cache_size;
#if defined(GEKKO) && !defined(PCSX2WII_JIT_DISABLE)
#define CODE_SLOTS 1024u
#define PC_SLOTS 2048u
typedef void (*iop_block)(uint32_t *,uint32_t);
typedef struct {uint32_t instr;iop_block fn;} owned_slot;
typedef struct {uint32_t pc,instr;iop_block fn;uint8_t rejected;} pc_slot;
#define BLOCK_SLOTS 128u
#define BLOCK_WORDS 8u
typedef unsigned (*iop_precise_block)(iop_state_t *,unsigned);
typedef struct {
 uint32_t pc,first,words[BLOCK_WORDS];
 iop_precise_block fn;
 uint8_t count;
} precise_slot;
static precise_slot block_cache[BLOCK_SLOTS];
static unsigned block_active;
/* Formation only peeks ordinary RAM/BIOS. No speculative MMIO reads. */
static int block_peek(iop_state_t *st,uint32_t pc,uint32_t *word)
{
 uint32_t phys=pc&0x1fffffffu;
 if(pc&3u)return 0;
 if(st->ram && phys<st->ram_size && st->ram_size-phys>=4u) {
  const uint8_t *p=st->ram+phys;
  *word=(uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
  return 1;
 }
 if(pc>=0xbfc00000u && st->bios && st->bios->data && phys>=0x1fc00000u &&
    phys-0x1fc00000u<st->bios->size && st->bios->size-(phys-0x1fc00000u)>=4u) {
  const uint8_t *p=st->bios->data+(phys-0x1fc00000u);
  *word=(uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
  return 1;
 }
 return 0;
}
static int block_control(uint32_t w)
{
 unsigned op=w>>26,fn=w&63u;
 return (op>=1u&&op<=7u)||(op==0u&&(fn==8u||fn==9u));
}
static owned_slot code_cache[CODE_SLOTS];
static pc_slot pc_cache[PC_SLOTS];
/* Unsupported translation depends on instruction bits, not guest PC. */
typedef struct {uint32_t instr;uint8_t valid;} rejected_slot;
static rejected_slot rejected_cache[64];
static iop_block find(uint32_t iw){unsigned h=(iw*2654435761u)&(CODE_SLOTS-1);for(unsigned n=0;n<CODE_SLOTS;n++){owned_slot*p=&code_cache[(h+n)&(CODE_SLOTS-1)];if(!p->fn)return NULL;if(p->instr==iw)return p->fn;}return NULL;}
static int insert(uint32_t iw,iop_block fn){if(cache_size==CODE_SLOTS)return 0;unsigned h=(iw*2654435761u)&(CODE_SLOTS-1);for(unsigned n=0;n<CODE_SLOTS;n++){owned_slot*p=&code_cache[(h+n)&(CODE_SLOTS-1)];if(!p->fn){p->instr=iw;p->fn=fn;cache_size++;return 1;}}return 0;}
#endif
int iop_jit_try_execute_one(iop_state_t *st,uint32_t pc,uint32_t iw)
{
#if !defined(GEKKO) || defined(PCSX2WII_JIT_DISABLE)
 (void)st;(void)pc;(void)iw;return 0;
#else
 if(!iop_core_native_call_safe(st,pc,iw))return 0;
 pc_slot*p=&pc_cache[((pc>>2)^(pc>>12))&(PC_SLOTS-1)];
 if(p->pc==pc&&p->instr==iw){if(p->rejected){rejected_hits++;return 0;}if(p->fn){p->fn(st->gpr,pc);executed++;return 1;}}
 rejected_slot *negative=&rejected_cache[(iw^(iw>>16))&63u];
 if(negative->valid && negative->instr==iw){
  p->pc=pc;p->instr=iw;p->fn=NULL;p->rejected=1;rejected_hits++;return 0;
 }
 iop_block fn=find(iw);int reject=0;
 if(!fn){
  if(cache_size==CODE_SLOTS)reject=1;
  else {
   ppc_codegen_ctx_t c;if(ppc_dynarec_init(&c,1))return 0;
   int result=ppc_dynarec_translate_iop_one(&c,iw);
   if(result){ppc_dynarec_free(&c);if(result==-2)return 0;reject=1;negative->instr=iw;negative->valid=1;}
   else {
    fn=(iop_block)ppc_dynarec_finalize(&c);
    if(!fn){ppc_dynarec_free(&c);return 0;}
    if(!insert(iw,fn)){ppc_dynarec_free(&c);return 0;}
   }
  }
 }
 p->pc=pc;p->instr=iw;p->fn=fn;p->rejected=(uint8_t)reject;
 if(reject)return 0;
 fn(st->gpr,pc);executed++;return 1;
#endif
}
unsigned iop_jit_try_execute_block(iop_state_t *st,unsigned budget)
{
#if !defined(GEKKO) || defined(PCSX2WII_JIT_DISABLE)
 (void)st;(void)budget;return 0;
#else
 if(!st||!budget||st->halted||st->idle||block_active)return 0;
 uint32_t pc=st->pc,first;
 if(!block_peek(st,pc,&first))return 0;
 precise_slot *slot=&block_cache[((pc>>2)^(pc>>5)^(pc>>12))&(BLOCK_SLOTS-1u)];
 int warm=slot->fn&&slot->pc==pc&&slot->first==first&&slot->count>0u&&slot->count<=BLOCK_WORDS;
 for(unsigned n=0;warm&&n<slot->count;n++) {
  uint32_t live;
  if(!block_peek(st,pc+n*4u,&live)||live!=slot->words[n])warm=0;
 }
 if(!warm) {
  uint32_t words[BLOCK_WORDS];unsigned count=0;int delay=0;
  for(;count<BLOCK_WORDS;count++) {
   if(!block_peek(st,pc+count*4u,&words[count]))break;
   if(delay){count++;break;}
   if(block_control(words[count]))delay=1;
  }
  if(!count)return 0;
  ppc_codegen_ctx_t ctx;
  if(ppc_dynarec_init(&ctx,(count*100u+160u+127u)/128u))return 0;
  unsigned native;
  if(ppc_dynarec_translate_iop_resident_block(&ctx,pc,words,count,
     (uint32_t)(uintptr_t)iop_core_block_prepare,
     (uint32_t)(uintptr_t)iop_core_block_retire,
     (uint32_t)(uintptr_t)iop_core_block_scalar,&native)) {
   ppc_dynarec_free(&ctx);return 0;
  }
  iop_precise_block fn=(iop_precise_block)ppc_dynarec_finalize(&ctx);
  if(!fn){ppc_dynarec_free(&ctx);return 0;}
  if(slot->fn)free((void*)slot->fn);else block_cache_size++;
  memset(slot,0,sizeof(*slot));
  slot->pc=pc;slot->first=first;slot->fn=fn;slot->count=(uint8_t)count;
  memcpy(slot->words,words,count*sizeof(uint32_t));
 }
 uint64_t stale=iop_core_block_stale_count();
 block_active=1;
 unsigned ticks=slot->fn(st,budget);
 block_active=0;
 block_runs++;block_ticks+=ticks;
 if(iop_core_block_stale_count()!=stale){free((void*)slot->fn);memset(slot,0,sizeof(*slot));block_cache_size--;}
 return ticks;
#endif
}
uint64_t iop_jit_get_block_runs(void){return block_runs;}
uint64_t iop_jit_get_block_ticks(void){return block_ticks;}
uint32_t iop_jit_get_block_cache_size(void){return block_cache_size;}
uint64_t iop_jit_get_executed_count(void){return executed;}
uint64_t iop_jit_get_rejected_hit_count(void){return rejected_hits;}
uint32_t iop_jit_get_cache_size(void){return cache_size;}
void iop_jit_reset_for_test(void){
#if defined(GEKKO) && !defined(PCSX2WII_JIT_DISABLE)
 if(block_active)return;
 for(unsigned n=0;n<BLOCK_SLOTS;n++)if(block_cache[n].fn)free((void*)block_cache[n].fn);
 memset(block_cache,0,sizeof block_cache);
 for(unsigned n=0;n<CODE_SLOTS;n++)if(code_cache[n].fn)free((void*)code_cache[n].fn);
 memset(code_cache,0,sizeof code_cache);memset(pc_cache,0,sizeof pc_cache);memset(rejected_cache,0,sizeof rejected_cache);
#endif
 executed=rejected_hits=block_runs=block_ticks=0;cache_size=block_cache_size=0;
}
