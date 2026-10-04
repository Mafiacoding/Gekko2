#include <assert.h>
#include <stdio.h>
#include "hw/iop_hle_thread.c"
int main(void)
{
 iop_state_t *st=iop_core_get_state();memset(st,0,sizeof(*st));iop_hle_thread_init();
 g.threads[0].in_use=1;g.threads[0].gpr[1]=0x12345678;
 st->gpr_generation=0xffffffffu;load_context(st,1);
 assert(st->gpr_generation==0&&st->gpr[1]==0x12345678);
 load_context(st,0);assert(st->gpr_generation==0);
 g.thread_count=1;g.alarms[0].in_use=1;g.alarms[0].deadline=0;
 g.alarms[0].common=0x55667788;g.alarms[0].handler=0x80100000;
 st->pc=0x80100004;st->next_pc=0x80100008;st->load_delay_reg=2;st->load_delay_value=0xaabbccdd;
 iop_hle_thread_tick(st);
 assert(st->gpr[4]==0x55667788&&st->gpr[2]==0xaabbccdd&&!st->load_delay_reg&&st->gpr_generation==2);
 puts("PASS real IOP context/Alarm replacement and pending-load flush invalidate resident copies");return 0;
}
