#include <stdio.h>
#include <string.h>
#include "core/ee/ee_hle_thread.c"
int main(void){
 ee_state_t st={0};ee_hle_thread_init();st.pc=0x1000;st.next_pc=0x1004;ensure_root_thread(&st);
 for(unsigned i=0;i<32;i++)st.fpr[i]=0x3f800000+i;
 st.fcr31=0x01800001;st.acc=0xc1200000;
 g.threads[1].in_use=1;g.threads[1].status=EE_THS_READY;g.threads[1].priority=2;g.threads[1].pc=0x2000;g.threads[1].next_pc=0x2004;g.thread_count=2;
 if(!ee_hle_thread_try_handle(&st,50,st.pc,0)||g.current_thread_id!=2)return puts("FAIL SleepThread did not switch"),1;
 for(unsigned i=0;i<32;i++)if(st.fpr[i])return puts("FAIL new thread inherited root FPR"),1;
 for(unsigned i=0;i<32;i++)st.fpr[i]=0x40000000+i;
 st.fcr31=0x01000001;st.acc=0x42c80000;st.gpr[4].ud0=1;
 if(!ee_hle_thread_try_handle(&st,51,st.pc,0)||g.current_thread_id!=1)return puts("FAIL WakeupThread did not preempt"),1;
 for(unsigned i=0;i<32;i++)if(st.fpr[i]!=0x3f800000+i)return puts("FAIL root FPR corrupted across preemption"),1;
 if(st.fcr31!=0x01800001||st.acc!=0xc1200000)return puts("FAIL root FCR31/ACC corrupted"),1;
 if(!ee_hle_thread_try_handle(&st,50,st.pc,0)||g.current_thread_id!=2)return puts("FAIL second SleepThread switch"),1;
 for(unsigned i=0;i<32;i++)if(st.fpr[i]!=0x40000000+i)return puts("FAIL child FPR not restored"),1;
 if(st.fcr31!=0x01000001||st.acc!=0x42c80000)return puts("FAIL child FCR31/ACC not restored"),1;
 puts("PASS FPU register/control/accumulator isolation through real sleep/wakeup preemption");return 0;
}
