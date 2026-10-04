/* Exercise the real GEKKO cache frontend with a controlled compiler backend.
 * Native hosts must never execute emitted PPC; actual compiled frontend and
 * real generated blocks are additionally checked by verify_ppc_cache_r1267.py. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/recompiler/ppc_dynarec.h"
static unsigned init_calls,translate_calls,free_calls,executions,fail_init,fail_finalize,fail_translate;
static const uint32_t unsupported=(28u<<26)|0x28u|(27u<<6); /* QFSRV */
static void native_block(ppc_dynarec_gpr128_t *p){(void)p;executions++;}
int ppc_dynarec_init(ppc_codegen_ctx_t*c,size_t n){(void)n;init_calls++;memset(c,0,sizeof *c);return fail_init?-1:0;}
int ppc_dynarec_translate_one(ppc_codegen_ctx_t*c,uint32_t i){(void)c;translate_calls++;return fail_translate?-2:i==unsupported?-1:0;}
ppc_block_fn ppc_dynarec_finalize(ppc_codegen_ctx_t*c){(void)c;return fail_finalize?NULL:native_block;}
void ppc_dynarec_free(ppc_codegen_ctx_t*c){(void)c;free_calls++;}
static void fake_cache_free(void*p){(void)p;free_calls++;}
#define GEKKO 1
/* The test backend uses host pointers; PPC pointer offsets are separately
 * asserted by the real 32-bit build and verified in Unicorn. */
#define _Static_assert(c,m) _Static_assert((c) || sizeof(void*) != 4,m)
#define free fake_cache_free
#include "core/recompiler/ee_jit.c"
#undef free
#undef _Static_assert
static ee_state_t state;
static unsigned failures,checks;
#define CHECK(c) do{checks++;if(!(c)){printf("FAIL: line %u: %s\n",__LINE__,#c);failures++;}}while(0)
static void reset(void){ee_jit_reset_stats_for_test();init_calls=translate_calls=free_calls=executions=fail_init=fail_finalize=fail_translate=0;}
int main(void){const unsigned pc=0x200000,good=(9u<<26)|(2u<<16)|7;
 reset();for(unsigned n=0;n<1000;n++)CHECK(!ee_jit_try_execute_one_at(&state,pc,unsupported));
 CHECK(init_calls==1&&translate_calls==1&&free_calls==1&&executions==0);
 CHECK(ee_jit_get_rejected_hit_count()==999&&ee_jit_get_compile_attempt_count()==1);
 CHECK(ee_jit_try_execute_one_at(&state,pc,good)==1);CHECK(executions==1&&ee_jit_get_cache_size()==1);
 CHECK(!ee_jit_try_execute_one_at(&state,pc,unsupported));CHECK(translate_calls==2); /* Changed word reuses exact-encoding rejection. */
 CHECK(ee_jit_try_execute_one_at(&state,pc,good)==1);CHECK(translate_calls==2&&executions==2); /* Positive owner survives rejection. */
 reset();CHECK(!ee_jit_try_execute_one_at(&state,pc,0x0000000c));CHECK(init_calls==0); /* SYSCALL fails prefilter. */
 CHECK(!ee_jit_try_execute_one_at(&state,pc,0x0000000c));CHECK(ee_jit_get_rejected_hit_count()==1);
 reset();CHECK(!ee_jit_try_execute_one_at(&state,0,unsupported));CHECK(!ee_jit_try_execute_one_at(&state,0,unsupported));CHECK(translate_calls==1); /* PC zero is a valid tag. */
 reset();fail_init=1;CHECK(!ee_jit_try_execute_one_at(&state,pc,good));fail_init=0;
 CHECK(ee_jit_try_execute_one_at(&state,pc,good)==1);CHECK(init_calls==2&&executions==1&&ee_jit_get_rejected_hit_count()==0);
 reset();fail_translate=1;CHECK(!ee_jit_try_execute_one_at(&state,pc,good));fail_translate=0;
 CHECK(ee_jit_try_execute_one_at(&state,pc,good)==1);CHECK(init_calls==2&&executions==1);
 reset();fail_finalize=1;CHECK(!ee_jit_try_execute_one_at(&state,pc,good));fail_finalize=0;
 CHECK(ee_jit_try_execute_one_at(&state,pc,good)==1);CHECK(init_calls==2&&free_calls==1&&executions==1);
 reset();CHECK(ee_jit_try_execute_one_at(&state,pc,good)==1);g_cache_count=EE_JIT_CACHE_SLOTS;
 CHECK(!ee_jit_try_execute_one_at(&state,pc+4,good+1));CHECK(!ee_jit_try_execute_one_at(&state,pc+4,good+1));
 CHECK(init_calls==1&&executions==1);CHECK(ee_jit_try_execute_one_at(&state,pc+8,good)==1);CHECK(executions==2&&init_calls==1);
 reset();CHECK(ee_jit_get_rejected_hit_count()==0&&ee_jit_get_compile_attempt_count()==0);
 CHECK(!ee_jit_try_execute_one_at(&state,pc,unsupported));CHECK(init_calls==1);
 reset();for(unsigned n=0;n<64;n++)CHECK(!ee_jit_try_execute_one_at(&state,pc+4*n,unsupported));
 CHECK(init_calls==1&&translate_calls==1&&free_calls==1);
 CHECK(ee_jit_try_execute_one_at(&state,pc,good));CHECK(executions==1);
 reset();CHECK(!ee_jit_try_execute_one_at(&state,pc,unsupported));CHECK(init_calls==1);
 printf("JIT rejection/ownership frontend: %u checks, %u failures\n",checks,failures);return !!failures;
}
