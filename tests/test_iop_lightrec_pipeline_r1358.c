#define main original_pipeline_test_main
#include "test_iop_pipeline_internal.c"
#undef main
#include "core/recompiler/iop_lightrec.h"
#include "core/recompiler/optimization.h"
int main(void)
{
 assert(!original_pipeline_test_main());
 gekko2_optimization_mask|=GEKKO2_OPT_BIT(GEKKO2_OPT_IOP_JIT);
 iop_lightrec_reset();setup();st->gpr[3]=3;
 put(0x100000,(0x23u<<26)|(1u<<21)|(2u<<16)); /* LW delayed r2 */
 put(0x100004,(2u<<21)|(3u<<16)|0x18u); /* MULT sees old r2=9 */
 step();step();assert(st->lo==27&&st->hi==0&&st->gpr[2]==0x12345678);
 assert(st->pc==0x80100008&&st->instructions_executed==2&&st->sched_ticks==2);
 assert(iop_lightrec_stat(1)==1);
 setup();st->gpr[2]=7;st->gpr[3]=11;
 put(0x100000,(4u<<26)|7u);put(0x100004,(2u<<21)|(3u<<16)|0x19u);
 step();step();assert(st->pc==0x80100020&&st->lo==77&&!st->branch_delay_pending);
 assert(st->instructions_executed==2&&st->sched_ticks==2&&iop_lightrec_stat(1)==2);
 /* The same guest address changes from MULT to DIV: synthetic source keys
  * use instruction bits and cannot resurrect a stale PPC result. */
 setup();st->gpr[2]=29;st->gpr[3]=4;put(0x100000,(2u<<21)|(3u<<16)|0x1bu);
 step();assert(st->lo==7&&st->hi==1&&iop_lightrec_stat(1)==3);
 /* Native block admission must execute Lightrec and consume exactly one
  * original tick, including the previous load's delayed writeback. */
 setup();st->gpr[3]=3;
 put(0x100000,(0x23u<<26)|(1u<<21)|(2u<<16));
 uint32_t mult=(2u<<21)|(3u<<16)|0x18u;put(0x100004,mult);
 step();
 assert(iop_core_block_prepare(st,0x80100004,mult,UINT32_MAX)==2);
 assert(st->lo==27 && st->gpr[2]==0x12345678);
 assert(st->pc==0x80100008 && st->instructions_executed==2 && st->sched_ticks==2);
 assert(iop_lightrec_stat(1)==4 && iop_lightrec_stat(5)==4 && !iop_lightrec_stat(6));
 /* A changed live word routes through prepared recovery, also using the
  * backend; no obsolete MULT body may run. */
 setup();st->gpr[2]=29;st->gpr[3]=4;
 uint32_t div=(2u<<21)|(3u<<16)|0x1bu;put(0x100000,div);
 assert(iop_core_block_prepare(st,0x80100000,mult,UINT32_MAX)==2);
 assert(st->lo==7 && st->hi==1 && st->instructions_executed==1 && st->sched_ticks==1);
 assert(iop_lightrec_stat(1)==5);
 setup();st->gpr[2]=7;st->gpr[3]=11;
 put(0x100000,(4u<<26)|7u);put(0x100004,mult);step();
 assert(iop_core_block_prepare(st,0x80100004,mult,UINT32_MAX)==2);
 assert(st->pc==0x80100020 && st->lo==77 && !st->branch_delay_pending);
 assert(st->instructions_executed==2 && st->sched_ticks==2 && iop_lightrec_stat(1)==6);
 iop_lightrec_reset();assert(!iop_lightrec_stat(0));
 puts("PASS Lightrec through real IOP retirement: pending LW, branch delay, exact ticks and changed source");
}
