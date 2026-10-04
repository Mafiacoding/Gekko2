#include <stdio.h>
#include <string.h>
#include "core/ee/ee_hle_thread.c"
int main(void){
 ee_state_t st={0};ee_hle_thread_init();
 st.pc=0x00257574;st.next_pc=st.pc+4;st.cop0[12]=0x70030c11;st.gpr[4].ud0=0x00100000;
 for(int i=0;i<3;i++){g.threads[i].in_use=1;g.threads[i].status=EE_THS_READY;}
 g.thread_count=3;g.current_thread_id=2;g.threads[1].status=EE_THS_RUN;
 g.threads[1].priority=30;g.threads[1].init_priority=30;
 g.threads[1].wait_type=EE_TSW_SEMA;g.threads[1].wait_id=5;g.threads[1].wakeup_count=8;
 g.semas[5].in_use=1;g.semas[5].wait_threads=2;
 ee_state_t before=st;ee_hle_thread_on_exec(&st);
 if(memcmp(&st,&before,sizeof(st)))return puts("FAIL Exec mirror changed CPU"),1;
 if(g.thread_count!=1||g.current_thread_id!=2||g.threads[0].in_use||g.threads[2].in_use)return puts("FAIL stale threads"),1;
 if(!g.threads[1].in_use||g.threads[1].status!=EE_THS_RUN||g.threads[1].priority||g.threads[1].init_priority||g.threads[1].wait_type||g.threads[1].wait_id||g.threads[1].wakeup_count)return puts("FAIL current thread lifecycle"),1;
 for(int i=0;i<EE_HLE_THREAD_MAX_SEMAS;i++)if(g.semas[i].in_use)return puts("FAIL stale sema"),1;
 if(alloc_tcb_slot()!=1||alloc_sema_slot()!=0)return puts("FAIL new object allocation"),1;
 ee_hle_thread_init();ee_hle_thread_on_exec(&st);
 if(g.thread_count||g.current_thread_id)return puts("FAIL cold exec creates phantom thread"),1;
 puts("PASS ExecPS2 HLE object lifecycle; CPU transfer left to BIOS");return 0;
}
