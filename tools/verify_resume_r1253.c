/* Read-only fresh diskless survey. No forced signals, PC changes, or GS writes. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/ee/ee_hle_thread.h"
#include "core/hw/gs.h"
#include "core/hw/gs_mem.h"
#include "core/hw/gif.h"
#include "core/hw/iop_sio2.h"
#include "core/hw/ee_intc.h"
#include "core/checkpoint.h"
static void scan(const char *label,uint32_t bp,uint32_t bw){
 unsigned nz=0, rgb=0;uint32_t mn=~0u,mx=0;
 for(unsigned y=0;y<224;y++)for(unsigned x=0;x<640;x++){
  uint32_t p=gs_mem_read_psmct32(bp,bw,x,y);nz+=p!=0;rgb+=(p&0xffffffu)!=0;
  if(p<mn)mn=p;if(p>mx)mx=p;
 }
 printf("PIXELS %s bp=%u bw=%u nonzero=%u rgb_nonblack=%u min=%08x max=%08x\n",label,bp,bw,nz,rgb,mn,mx);
}
int main(int argc,char **argv){
 if(argc<2){fprintf(stderr,"usage: %s bios [million EE instructions]\n",argv[0]);return 2;}
 setvbuf(stdout,NULL,_IOLBF,0);bios_image_t b;
 if(bios_load(argv[1],&b)||system_init(&b,&b))return 3;
 gs_mem_init();iop_sio2_pad_connect();
 if(argc>3 && checkpoint_load(argv[3],&b,&b,0))return 4;
 ee_state_t *s=ee_core_get_state();gs_state_t *g=gs_get_state();gif_state_t *f=gif_get_state();
 uint64_t budget=(argc>2?strtoull(argv[2],0,10):2000)*1000000ull+s->instructions_executed;
 while(s->instructions_executed<budget&&!s->halted){
  system_run_interleaved(1000000);
  uint64_t rc[4],a,c,h,re;uint32_t rl[6],cd,se;int use;int32_t max,cnt,wait;
  ee_core_get_r1249_repair(rc,rl);ee_core_get_r1190_sif_diag(&a,&c,&h,&re,&cd,&se);
  ee_hle_thread_get_sema_state(5,&use,&max,&cnt,&wait);
  printf("BOOT n=%llu pc=%08x tid=%d REND=%llu repair=%llu S5=%d/%d w=%d sig=%llu pmode=%llx qw=%llu sprite=%llu tri=%llu\n",(unsigned long long)s->instructions_executed,s->pc,ee_hle_thread_get_current_thread_id(),(unsigned long long)re,(unsigned long long)rc[0],cnt,max,wait,(unsigned long long)ee_hle_thread_get_signal_calls(5),(unsigned long long)g->pmode,(unsigned long long)f->quadwords_seen,(unsigned long long)f->sprites_drawn,(unsigned long long)f->triangles_drawn);
 }
 for(int t=1;t<=ee_hle_thread_get_thread_count();t++)printf("THREAD %d status=%u prio=%u pc=%08x wait=%u/%u\n",t,ee_hle_thread_get_status(t),ee_hle_thread_get_priority(t),ee_hle_thread_get_saved_pc(t),ee_hle_thread_get_wait_type(t),ee_hle_thread_get_wait_id(t));
 ee_intc_state_t *ic=ee_intc_get_state();
 printf("CPU status=%08x cause=%08x EPC=%08x BadVA=%08x INTC=%08x/%08x SP=%08x RA=%08x\n",s->cop0[12],s->cop0[13],s->cop0[14],s->cop0[8],ic->stat,ic->mask,(uint32_t)s->gpr[29].ud0,(uint32_t)s->gpr[31].ud0);
 for(int k=0;k<16;k++)printf("SEMA %d signals=%llu\n",k,(unsigned long long)ee_hle_thread_get_signal_calls(k));
 if(argc>4) printf("CHECKPOINT result=%d\n",checkpoint_save(argv[4]));
 uint32_t bp=(g->dispfb2&511u)*2048u,bw=((g->dispfb2>>9)&63u)*64u;if(!bw)bw=640;
 printf("GS pmode=%llx dispfb2=%llx display2=%llx fbp=%u ctx1=%u\n",(unsigned long long)g->pmode,(unsigned long long)g->dispfb2,(unsigned long long)g->display2,f->fbp,f->ctx1_fbp);
 scan("DISPFB2",bp,bw);scan("GIF",f->fbp,f->fbw?f->fbw:640);
 FILE *image=fopen("work/r1253-gs.ppm","wb");
 if(image){fprintf(image,"P6\n640 448\n255\n");for(unsigned y=0;y<448;y++)for(unsigned x=0;x<640;x++){uint32_t p=gs_mem_read_psmct32(bp,bw,x,y);unsigned char c[3]={(unsigned char)p,(unsigned char)(p>>8),(unsigned char)(p>>16)};fwrite(c,1,3,image);}fclose(image);}
 printf("HALT=%d reason=%s\n",s->halted,s->halt_reason);return s->halted?1:0;
}
