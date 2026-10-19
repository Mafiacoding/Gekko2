/* Private-image native diagnostic. Uses the launcher's actual cold disc boot.
 * Never patches guest code or fabricates PC/register/video progress. */
#include "core/checkpoint.h"
#include "core/iso_loader.h"
#include "core/recompiler/vu_jit.h"
#include "core/ee/ee_hle_thread.h"
#include "core/hw/sif.h"
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/iop/iop_core.h"
#include "core/hw/gs.h"
#include "core/hw/gif.h"
#include "core/hw/vu.h"
#include "core/hw/vif.h"
#include "core/hw/ipu.h"
#include "core/hw/iop_cdvd.h"
#include "core/hw/iop_cdrom_legacy.h"
#include "core/recompiler/optimization.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include "core/hw/gs_mem.h"
#include "core/hw/gs_wii_output.h"
#include "core/hw/iop_sio2.h"
#include "core/hw/dma.h"
#include "core/hw/vif.h"
#include <time.h>
static void capture(const char *dir,unsigned slice) {
 gs_state_t *g=gs_get_state();
 for(unsigned c=0;c<2;c++) {
  if(!(g->pmode&(1u<<c)))continue;
  uint64_t fb=c?g->dispfb2:g->dispfb1,display=c?g->display2:g->display1;
  uint32_t bp,bw,x,y,w,h;gs_decode_dispfb(fb,&bp,&bw);gs_decode_display_region(fb,display,g->smode2,&x,&y,&w,&h);
  if(!w||!h||w>1024||h>1024||!bw)continue;
  char path[1024];snprintf(path,sizeof path,"%s/frame-%04u-%u.ppm",dir,slice,c);FILE *f=fopen(path,"wb");if(!f)exit(7);
  fprintf(f,"P6\n%u %u\n255\n",w,h);unsigned rgb=0,psm=(fb>>15)&31u;
  for(unsigned row=0;row<h;row++)for(unsigned col=0;col<w;col++) {
   uint32_t pixel;
   if(psm==2||psm==10){uint16_t p=psm==10?gs_mem_read_psmct16s(bp,bw,x+col,y+row):gs_mem_read_psmct16(bp,bw,x+col,y+row);pixel=((p&31)<<3)|(((p>>5)&31)<<11)|(((p>>10)&31)<<19);}
   else if(psm==0||psm==1)pixel=gs_mem_read_psmct32(bp,bw,x+col,y+row);else pixel=0;
   unsigned char bytes[3]={pixel,pixel>>8,pixel>>16};rgb+=!!(pixel&0xffffff);fwrite(bytes,1,3,f);
  }
  fclose(f);printf("FRAME %s RGB=%u psm=%u %ux%u\n",path,rgb,psm,w,h);
 }
}

extern void ee_core_get_r1252_fault(uint32_t out[112]);
int main(int argc,char **argv)
{
 if(argc<4){fprintf(stderr,"usage: %s BIOS ISO IOP-million-slices [private-checkpoint]\n",argv[0]);return 2;}setvbuf(stdout,0,_IOLBF,0);
 const char *options=getenv("GEKKO2_OPTIONS");
 if(options){
  char *end;errno=0;unsigned long mask=strtoul(options,&end,16);
  if(!*options||isspace((unsigned char)*options)||*options=='-'||*options=='+'||
     errno||*end||mask>UINT32_MAX){fprintf(stderr,"Invalid GEKKO2_OPTIONS mask\n");return 2;}
  gekko2_optimization_mask=(uint32_t)mask;
 }
 printf("OPTIONS mask=%08x forced_scheduler_bits=0 host_PPC_execution=0\n",gekko2_optimization_mask);
 bios_image_t bios;if(bios_load(argv[1],&bios)||system_init(&bios,&bios))return 3;
 if(argv[2][0]!='-'){
  iso_image_t disc;iso_dirent_t cnf;uint8_t bytes[2049];
  if(iso_open(argv[2],&disc))return 4;
  printf("DISC stride=%u offset=%u root_lba=%u root_bytes=%u\n",disc.physical_stride,disc.data_offset,disc.root_lba,disc.root_size);
  if(!iso_find_path(&disc,"SYSTEM.CNF;1",&cnf)&&!iso_read_sector(&disc,cnf.lba,bytes)){
   bytes[cnf.size<2048?cnf.size:2048]=0;printf("SYSTEM.CNF: %s\n",bytes);
  }
  iso_close(&disc);
  int a=iop_cdvd_mount_iso(argv[2]),b=iop_cdrom_legacy_mount_iso(argv[2]);
  if(a&&b)return 4;if(!a)iop_cdvd_set_disc_present(0x12);
 }
 const char *resume=getenv("GEKKO2_RESUME");if(resume&&checkpoint_load(resume,&bios,&bios,argv[2]))return 8;
 const char *dir=getenv("GEKKO2_CAPTURE"),*control=getenv("GEKKO2_INPUT"),*plan=getenv("GEKKO2_PAD_PLAN");
 if(control||plan)iop_sio2_pad_connect();
 unsigned slices=strtoul(argv[3],0,0);
 clock_t start=clock();
 for(unsigned n=0;n<slices;n++){
  if(control){FILE *f=fopen(control,"r");unsigned buttons=0;if(f){fscanf(f,"%x",&buttons);fclose(f);}iop_sio2_pad_set_buttons(buttons);}
  if(plan){
   FILE *f=fopen(plan,"r");unsigned mask=0,buttons;unsigned long long first,last;
   uint64_t tick=iop_core_get_state()->sched_ticks;
   if(f){while(fscanf(f,"%llu %llu %x",&first,&last,&buttons)==3)
    if(tick>=first&&tick<last)mask|=buttons;fclose(f);}
   iop_sio2_pad_set_buttons((uint16_t)mask);
  }
  system_run_interleaved(1000000);
  ee_state_t *e=ee_core_get_state();iop_state_t *i=iop_core_get_state();gs_state_t *g=gs_get_state();ipu_profile_t p;ipu_get_profile(&p);
  iop_cdvd_checkpoint_t cd;iop_cdvd_checkpoint_save(&cd);
  printf("SURVEY slice=%u EE=%llu PC=%08x IOP=%llu PC=%08x halted=%u/%u PMODE=%llx GIF=%llu VU=%llu CDVD_N=%llu CDVD_S=%llu IPU_input=%llu busy=%u last=%08x unimplemented=%llu LOADFILE_version=%08x\n",n,
  (unsigned long long)e->instructions_executed,e->pc,(unsigned long long)i->instructions_executed,i->pc,e->halted,i->halted,
  (unsigned long long)g->pmode,(unsigned long long)gif_get_state()->quadwords_seen,(unsigned long long)vu1_get_state()->instructions_executed,
  (unsigned long long)cd.ncmd_call_count,(unsigned long long)cd.scmd_call_count,(unsigned long long)p.accepted_qwc,p.busy,p.last_command,(unsigned long long)p.unimplemented_commands,e->loadfile_rpc_version);
  if(dir&&(n%5==4||n+1==slices))capture(dir,n);
  dma_state_t *d=dma_get_state();vu1_state_t *v=vu1_get_state();
  if(getenv("GEKKO2_DMA_TRACE")){
   vif_state_t *vf=vif1_get_state();printf("VIF1 code=%08x stat=%08x irq=%u stall=%u direct=%u/%u codes=%llu unpack=%llu\n",vf->code,vf->stat,vf->irq_stalled,vf->stall_words,vf->direct_have_words,vf->direct_needed_words,(unsigned long long)vf->codes_processed,(unsigned long long)vf->unpack_vectors_written);
   for(unsigned c=0;c<10;c++)printf("DMA%u chcr=%08x madr=%08x qwc=%08x tadr=%08x sadr=%08x err=%u transferred=%u\n",c,d->chan[c].chcr,d->chan[c].madr,d->chan[c].qwc,d->chan[c].tadr,d->chan[c].sadr,d->chan[c].last_error,d->chan[c].quadwords_transferred);
   printf("DMAC ctrl=%08x enable=%08x stat=%08x rbsr=%08x rbor=%08x stadr=%08x vif_pending=%u/%u scratch=%08x ptr=%08x\n",d->d_ctrl,d->d_enable_state,d->d_stat,d->d_rbsr,d->d_rbor,d->d_stadr,d->vif_tag_pending[0],d->vif_tag_pending[1],((uint32_t)e->scratch[20]|((uint32_t)e->scratch[21]<<8)|((uint32_t)e->scratch[22]<<16)|((uint32_t)e->scratch[23]<<24)),(uint32_t)e->gpr[4].ud0);
  }
  printf("DETAIL count=%08x status=%08x cause=%08x vu0run=%u vu1run=%u kick=%u dma=%08x/%08x/%08x fifo=%u pad_samples=%llu pad_cmd=%llu cpu_s=%.3f\n",e->cop0[9],e->cop0[12],e->cop0[13],e->vu0_running,v->running,v->xgkick_pending,d->chan[0].chcr,d->chan[1].chcr,d->chan[2].chcr,gif_get_state()->fifo_count,(unsigned long long)iop_sio2_pad_sample_count(),(unsigned long long)iop_sio2_get_pad_command_count(),(double)(clock()-start)/CLOCKS_PER_SEC);
  if((n%10==9||n+1==slices||e->halted||i->halted) && argc>4){
   printf("CHECKPOINT rc=%d path=%s\n",checkpoint_save(argv[4]),argv[4]);
   printf("SIF_INIT=%u BINDS=%u current_thread=%d\n",sif_cmd_iop_get_init_cmd_count(),sif_cmd_iop_get_rpc_bind_count(),ee_hle_thread_get_current_thread_id());
   uint32_t cd[64],sid[64];unsigned count=sif_cmd_iop_dump_bind_table(cd,sid,64);
   for(unsigned j=0;j<count;j++)printf("BIND cd=%08x sid=%08x\n",cd[j],sid[j]);
  }
  if(e->halted||i->halted){printf("HALT %s | %s\n",e->halt_reason,i->halt_reason);break;}
 }
 uint32_t fault[112];ee_core_get_r1252_fault(fault);for(unsigned n=0;n<112;n++)printf("FAULT %u=%08x\n",n,fault[n]);
 for(unsigned r=0;r<32;r++)printf("GPR %u=%016llx/%016llx\n",r,(unsigned long long)ee_core_get_state()->gpr[r].ud0,(unsigned long long)ee_core_get_state()->gpr[r].ud1);
 for(unsigned c=0;c<32;c++)printf("COP0 %u=%08x\n",c,ee_core_get_state()->cop0[c]);
 ipu_profile_t p;ipu_get_profile(&p);for(unsigned n=0;n<10;n++)printf("IPU_COMMAND %u=%llu\n",n,(unsigned long long)p.commands[n]);
 ee_state_t *e=ee_core_get_state();iop_state_t *i=iop_core_get_state();
 {
#ifdef GEKKO2_IDLE_SKIP
 extern uint64_t ee_idle_skip_stats[16],ee_dma_diag_hits;printf("IDLE_SKIP skips=%llu slots=%llu pc_seen=%llu no_vu=%llu no_rpc=%llu no_irq=%llu no_timer=%llu dmaw_hits=%llu\n",(unsigned long long)ee_idle_skip_stats[0],(unsigned long long)ee_idle_skip_stats[1],(unsigned long long)ee_idle_skip_stats[2],(unsigned long long)ee_idle_skip_stats[3],(unsigned long long)ee_idle_skip_stats[4],(unsigned long long)ee_idle_skip_stats[5],(unsigned long long)ee_idle_skip_stats[6],(unsigned long long)ee_dma_diag_hits);
#endif
 }
 {
#ifdef GEKKO2_SPEED_STUB
 extern uint64_t ee_quiet_arms,ee_quiet_arm_zero,ee_quiet_hits,ee_speed_retire_stats[2],ee_arm_why[12];printf("QUIET arms=%llu zero=%llu hits=%llu retire_lite=%llu full=%llu why:",(unsigned long long)ee_quiet_arms,(unsigned long long)ee_quiet_arm_zero,(unsigned long long)ee_quiet_hits,(unsigned long long)ee_speed_retire_stats[0],(unsigned long long)ee_speed_retire_stats[1]);for(int q=0;q<12;q++)printf(" %llu",(unsigned long long)ee_arm_why[q]);printf("\n");
#endif
 }
#ifdef GEKKO2_IDLE_SKIP
 {extern uint64_t ee_spin_stats[4];printf("SPIN hashes=%llu confirmed=%llu skips=%llu slots=%llu\n",(unsigned long long)ee_spin_stats[0],(unsigned long long)ee_spin_stats[1],(unsigned long long)ee_spin_stats[2],(unsigned long long)ee_spin_stats[3]);}
#endif
 {extern uint64_t system_event_stat(unsigned);for(int q=0;q<5;q++)printf("EVSTAT%d=%llu\n",q,(unsigned long long)system_event_stat(q));}
 {extern uint64_t ee_event_deadline_stats[8];printf("DEADLINE");for(int q=0;q<8;q++)printf(" %llu",(unsigned long long)ee_event_deadline_stats[q]);printf("\n");}
 printf("IOP_TICKS=%llu\n",(unsigned long long)i->sched_ticks);
 printf("FINAL current_thread=%d threads=%d VU_unknown=%llu timed_JIT_blocks=%llu host_PPC_execution=0\n",ee_hle_thread_get_current_thread_id(),ee_hle_thread_get_thread_count(),(unsigned long long)vu1_get_state()->unimplemented_opcodes_seen,(unsigned long long)vu_jit_get_timed_block_count());
 for(int t=1;t<=ee_hle_thread_get_thread_count();t++)printf("THREAD id=%d status=%u priority=%u pc=%08x wait=%u/%u\n",t,ee_hle_thread_get_status(t),ee_hle_thread_get_priority(t),ee_hle_thread_get_saved_pc(t),ee_hle_thread_get_wait_type(t),ee_hle_thread_get_wait_id(t));
 for(int k=0;k<16;k++){int use;int32_t maximum,count,wait;ee_hle_thread_get_sema_state(k,&use,&maximum,&count,&wait);if(use)printf("SEMA id=%d count=%d max=%d wait=%d signals=%llu\n",k,count,maximum,wait,(unsigned long long)ee_hle_thread_get_signal_calls(k));}
 return e->halted||i->halted?1:0;
}
