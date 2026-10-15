/* Real cold disc boot and controller input; no guest patches or forced progress. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/ee/ee_hle_thread.h"
#include "core/iop/iop_core.h"
#include "core/hw/iop_cdvd.h"
#include "core/hw/iop_cdrom_legacy.h"
#include "core/hw/iop_sio2.h"
#include "core/hw/gs.h"
#include "core/hw/gs_mem.h"
#include "core/hw/gs_wii_output.h"
#include "core/hw/ipu.h"
#include "core/hw/gif.h"
#include "core/hw/vu.h"
#include "core/checkpoint.h"
#include "core/recompiler/optimization.h"
#include "core/recompiler/iop_lightrec.h"
static void capture(const char *directory,uint64_t instructions){
 gs_state_t *g=gs_get_state();uint64_t df=(g->pmode&1)?g->dispfb1:g->dispfb2,display=(g->pmode&1)?g->display1:g->display2;
 uint32_t bp,bw,x,y,w,h;gs_decode_dispfb(df,&bp,&bw);gs_decode_display_region(df,display,g->smode2,&x,&y,&w,&h);
 if(!(g->pmode&3)||!bw||!w||!h||w>1024||h>512)return;
 char path[512];snprintf(path,sizeof path,"%s/tekken-%010llu.ppm",directory,(unsigned long long)instructions);FILE *out=fopen(path,"wb");if(!out)return;
 fprintf(out,"P6\n%u %u\n255\n",w,h);
 for(unsigned row=0;row<h;row++)for(unsigned col=0;col<w;col++){uint32_t p=gs_mem_read_psmct32(bp,bw,x+col,y+row);uint8_t rgb[3]={p,p>>8,p>>16};fwrite(rgb,1,3,out);}
 fclose(out);printf("CAPTURE %s PMODE=%llx BP=%u BW=%u XY=%u,%u WH=%u,%u\n",path,(unsigned long long)g->pmode,bp,bw,x,y,w,h);
}
int main(int argc,char **argv){
 if(argc!=5&&argc!=6)return 2;setvbuf(stdout,0,_IOLBF,0);bios_image_t bios;
 if(bios_load(argv[1],&bios)||system_init(&bios,&bios))return 3;
 if(iop_cdvd_mount_iso(argv[2])||iop_cdrom_legacy_mount_iso(argv[2]))return 4;iop_cdvd_set_disc_present(0x12);
 if(argc==6&&checkpoint_load(argv[5],&bios,&bios,argv[2]))return 6;
 gekko2_optimization_mask|=(1u<<14)|(1u<<28);
#ifdef GEKKO2_IOP_LIGHTREC_PILOT
 /* Explicit native backend verification only; host availability masks PPC off. */
 gekko2_optimization_mask|=GEKKO2_OPT_BIT(GEKKO2_OPT_IOP_JIT);
#endif
 iop_sio2_pad_connect();
 printf("BEGIN EE=%llu IOP=%llu\n",(unsigned long long)ee_core_get_state()->instructions_executed,(unsigned long long)iop_core_get_state()->instructions_executed);
 uint64_t saved_at=0, stalled=0,loops=0;
 uint64_t limit=strtoull(argv[3],0,0),next=(ee_core_get_state()->instructions_executed/100000000ull+1u)*100000000ull;
 while(ee_core_get_state()->instructions_executed<limit){
  ee_state_t *e=ee_core_get_state();iop_state_t *i=iop_core_get_state();uint64_t n=e->instructions_executed;
  /* Pulse START / SELECT / CROSS separately through the ordinary pad path. */
  uint16_t buttons=0;unsigned input_phase=(n/30000000ull)%6;
  if(gs_get_state()->pmode==0x7f67u){if(input_phase==1)buttons=IOP_PAD_BTN_START;else if(input_phase==3)buttons=IOP_PAD_BTN_SELECT;else if(input_phase==5)buttons=IOP_PAD_BTN_CROSS;}iop_sio2_pad_set_buttons(buttons);
  system_run_interleaved(1000000);
  if(++loops%10==0){printf("HEART EE=%llu IOP=%llu pc=%08x idle=%u clock=%llu\n",(unsigned long long)e->instructions_executed,(unsigned long long)i->instructions_executed,e->pc,e->idle,(unsigned long long)ee_core_display_clock_save());}
  if(e->instructions_executed-n<1000000u){if(++stalled%10==0){printf("STALL EE=%llu PC=%08x IOP=%llu PC=%08x clock=%llu tid=%d\n",(unsigned long long)n,e->pc,(unsigned long long)i->instructions_executed,i->pc,(unsigned long long)ee_core_display_clock_save(),ee_hle_thread_get_current_thread_id());for(int t=1;t<=ee_hle_thread_get_thread_count();t++)printf("THREAD %d status=%u pc=%08x wait=%u/%u\n",t,ee_hle_thread_get_status(t),ee_hle_thread_get_saved_pc(t),ee_hle_thread_get_wait_type(t),ee_hle_thread_get_wait_id(t));}if(stalled==20){char cp[512];snprintf(cp,sizeof cp,"%s/stall.ckpt",argv[4]);checkpoint_save(cp);return 7;}}else stalled=0;
  if(e->instructions_executed>=next){
   ipu_profile_t p;ipu_get_profile(&p);
   printf("SURVEY EE=%llu PC=%08x IOP=%llu PC=%08x halted=%u/%u input=%04x pad_commands=%u pad_samples=%u PMODE=%llx IPU=%llu/%llu\n",(unsigned long long)e->instructions_executed,e->pc,(unsigned long long)i->instructions_executed,i->pc,e->halted,i->halted,buttons,iop_sio2_get_pad_command_count(),iop_sio2_pad_sample_count(),(unsigned long long)gs_get_state()->pmode,(unsigned long long)p.accepted_qwc,(unsigned long long)p.completed_commands);
   vu1_state_t *v=vu1_get_state();printf("GEOMETRY VU0=%llu missing=%llu VU1=%llu missing=%llu tri=%llu sprites=%llu deg=%llu\n",(unsigned long long)e->vu0_instructions_executed,(unsigned long long)e->vu0_unimplemented_opcodes_seen,(unsigned long long)v->instructions_executed,(unsigned long long)v->unimplemented_opcodes_seen,(unsigned long long)gif_get_state()->triangles_drawn,(unsigned long long)gif_get_state()->sprites_drawn,(unsigned long long)gif_get_gouraud_stat(GIF_GOURAUD_DEGENERATE));
   gif_state_t *f=gif_get_state();printf("RENDER rgba=%08x frame=%u/%u psm=%u bg=%llx pixel0=%08x blend=%u,%u,%u,%u fix=%u\n",f->rgba,f->fbp,f->fbw,f->frame_psm,(unsigned long long)gs_get_state()->bgcolor,gs_mem_read_psmct32((gs_get_state()->dispfb1&511)*2048,640,0,0),f->alpha_a,f->alpha_b,f->alpha_c,f->alpha_d,f->alpha_fix);
   capture(argv[4],e->instructions_executed);next+=100000000ull;
  }
  if(e->instructions_executed/500000000ull>saved_at){saved_at=e->instructions_executed/500000000ull;char save[512];snprintf(save,sizeof save,"%s/milestone-%llu.ckpt",argv[4],(unsigned long long)saved_at);printf("MILESTONE %d\n",checkpoint_save(save));}
  if(e->halted||i->halted){printf("HALT EE=%llu PC=%08x OP=%08x %s | %s\n",(unsigned long long)e->instructions_executed,e->pc,ee_mem_read32(e,e->pc),e->halt_reason,i->halt_reason);char haltpath[512];snprintf(haltpath,sizeof haltpath,"%s/halt.ckpt",argv[4]);printf("HALT_CHECKPOINT %d\n",checkpoint_save(haltpath));return 5;}
 }
 printf("END EE=%llu IOP=%llu halted=%u/%u LIGHTREC ready=%llu bodies=%llu misses=%llu failures=%llu ISO requests=%llu hits=%llu fills=%llu\n",(unsigned long long)ee_core_get_state()->instructions_executed,(unsigned long long)iop_core_get_state()->instructions_executed,ee_core_get_state()->halted,iop_core_get_state()->halted,(unsigned long long)iop_lightrec_stat(0),(unsigned long long)iop_lightrec_stat(1),(unsigned long long)iop_lightrec_stat(2),(unsigned long long)iop_lightrec_stat(3),(unsigned long long)iso_cache_stat(0),(unsigned long long)iso_cache_stat(1),(unsigned long long)iso_cache_stat(2));
 capture(argv[4],ee_core_get_state()->instructions_executed);
 char checkpoint[512];snprintf(checkpoint,sizeof checkpoint,"%s/state.ckpt",argv[4]);printf("CHECKPOINT %d\n",checkpoint_save(checkpoint));
 return 0;
}
