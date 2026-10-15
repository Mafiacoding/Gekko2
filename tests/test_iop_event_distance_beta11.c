#include "hw/iop_hle_thread.c"
#include <assert.h>
#include <stdio.h>
int main(void){
 iop_state_t s={0};s.instructions_executed=100;
 iop_hle_thread_init();gekko2_optimization_mask|=GEKKO2_OPT_BIT(GEKKO2_OPT_IOP_DEADLINES);
 g.thread_count=1;g.threads[0].in_use=1;g.threads[0].status=IOP_THS_WAIT;
 g.threads[0].wait_type=IOP_TSW_DELAY;g.threads[0].delay_deadline=125;
 assert(iop_hle_thread_event_distance(&s,64)==25);
 uint32_t size;iop_hle_thread_get_checkpoint_blob(&size);
 g.threads[0].delay_deadline=108;assert(iop_hle_thread_event_distance(&s,64)==8);
 g.alarms[0].in_use=1;g.alarms[0].deadline=103;
 assert(iop_hle_thread_event_distance(&s,64)==3);
 g.alarm_in_dispatch=1;assert(iop_hle_thread_event_distance(&s,64)==8);
 s.instructions_executed=108;assert(iop_hle_thread_event_distance(&s,64)==1);
 g.threads[0].wait_type=IOP_TSW_SEMA;assert(iop_hle_thread_event_distance(&s,64)==64);
 puts("PASS BETA11 event distance: uncached/exposed deadlines and dispatch ordering");return 0;
}
