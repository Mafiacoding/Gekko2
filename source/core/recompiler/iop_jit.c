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
#if defined(GEKKO) && !defined(PCSX2WII_JIT_DISABLE)
#define CODE_SLOTS 1024u
#define PC_SLOTS 2048u
typedef void (*iop_block)(uint32_t *,uint32_t);
typedef struct {uint32_t instr;iop_block fn;} owned_slot;
typedef struct {uint32_t pc,instr;iop_block fn;uint8_t rejected;} pc_slot;
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
uint64_t iop_jit_get_executed_count(void){return executed;}
uint64_t iop_jit_get_rejected_hit_count(void){return rejected_hits;}
uint32_t iop_jit_get_cache_size(void){return cache_size;}
void iop_jit_reset_for_test(void){
#if defined(GEKKO) && !defined(PCSX2WII_JIT_DISABLE)
 for(unsigned n=0;n<CODE_SLOTS;n++)if(code_cache[n].fn)free((void*)code_cache[n].fn);
 memset(code_cache,0,sizeof code_cache);memset(pc_cache,0,sizeof pc_cache);memset(rejected_cache,0,sizeof rejected_cache);
#endif
 executed=rejected_hits=0;cache_size=0;
}
