#include <assert.h>
#include <stdio.h>
#include "core/ee/ee_hle_thread.c"
int main(void) {
 for(unsigned gate=0;gate<3;gate++) {
  ee_state_t st={0};ee_hle_thread_init();
  st.pc=0x1000;st.next_pc=0x1004;st.cop0[12]=0x10001;
  st.gpr[29].ud0=0x1ff000;st.gpr[31].ud0=0x1100;
  st.gpr[18].ud0=0x1122334455667788ULL;st.gpr[18].ud1=0x99aabbccddeeff00ULL;
  ensure_root_thread(&st);g.threads[0].priority=30;
  ee_tcb_t *child=&g.threads[1];child->in_use=1;child->status=EE_THS_WAIT;
  child->wait_type=EE_TSW_SLEEP;child->priority=10;child->pc=0x2000;child->next_pc=0x2004;
  child->gpr[29].ud0=0x2ff000;g.thread_count=2;
  ee_state_t interrupted=st;
  st.pc=0x3000;st.next_pc=0x3004;st.gpr[29].ud0=0x3ff000;st.gpr[4].ud0=2;
  st.cop0[12]=gate==0?0x10000:gate==1?0x10002:0x10001;
  assert(ee_hle_thread_try_handle(&st,-52,0x3000,0));
  assert(g.current_thread_id==1&&child->status==EE_THS_READY);
  assert(st.pc==0x3004&&st.gpr[29].ud0==0x3ff000);
  assert(g.threads[0].pc==0x1000&&g.threads[0].gpr[29].ud0==0x1ff000);
  if(gate<2) {
   ee_hle_thread_check_preempt(&st);assert(g.current_thread_id==1);
   st=interrupted;st.gpr[4].ud0=2;
  }
  ee_hle_thread_check_preempt(&st);
  assert(g.current_thread_id==2&&st.pc==0x2000&&st.gpr[29].ud0==0x2ff000);
  if(gate<2) {
   assert(g.threads[0].pc==0x1000&&g.threads[0].gpr[29].ud0==0x1ff000);
   assert(g.threads[0].gpr[18].ud0==0x1122334455667788ULL);
   assert(g.threads[0].gpr[18].ud1==0x99aabbccddeeff00ULL);
  }
 }
 puts("PASS iWakeup readiness, callback isolation and safe interrupt-return preemption");
 return 0;
}
