/* Fresh physical-disc boot survey. No forced signals, PC changes, or GS writes. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/iop/iop_core.h"
#include "core/ee/ee_hle_thread.h"
#include "core/hw/gs.h"
#include "core/hw/gs_mem.h"
#include "core/hw/gif.h"
#include "core/hw/iop_sio2.h"
#include "core/hw/ee_intc.h"
#include "core/checkpoint.h"
#include "core/hw/iop_cdvd.h"
#include "core/hw/dma.h"
#include "core/hw/gs_wii_output.h"
#include "core/iso_loader.h"
#include "core/hw/iop_cdrom_legacy.h"
static void scan(const char *label,uint32_t bp,uint32_t bw){
 unsigned nz=0, rgb=0;uint32_t mn=~0u,mx=0;
 for(unsigned y=0;y<224;y++)for(unsigned x=0;x<640;x++){
  uint32_t p=gs_mem_read_psmct32(bp,bw,x,y);nz+=p!=0;rgb+=(p&0xffffffu)!=0;
  if(p<mn)mn=p;if(p>mx)mx=p;
 }
 printf("PIXELS %s bp=%u bw=%u nonzero=%u rgb_nonblack=%u min=%08x max=%08x\n",label,bp,bw,nz,rgb,mn,mx);
}
int main(int argc,char **argv){
 if(argc<3){fprintf(stderr,"usage: %s bios disc [million EE instructions] [checkpoint]\n",argv[0]);return 2;}
 setvbuf(stdout,NULL,_IOLBF,0);bios_image_t b;
 if(bios_load(argv[1],&b)||system_init(&b,&b))return 3;
 gs_mem_init();iop_sio2_pad_connect();
 iso_image_t disc; if(iso_open(argv[2],&disc)){fprintf(stderr,"DISC: invalid/unsupported image\n");return 4;}
 printf("DISC stride=%u data_offset=%u root_lba=%u root_bytes=%u\n",disc.physical_stride,disc.data_offset,disc.root_lba,disc.root_size);
 iso_dirent_t cnf; uint8_t buf[2049];
 if((iso_find_in_root(&disc,"SYSTEM.CNF;1",&cnf)==0||iso_find_in_root(&disc,"SYSTEM.CNF",&cnf)==0)&&iso_read_sector(&disc,cnf.lba,buf)==0){buf[cnf.size<2048?cnf.size:2048]=0;printf("SYSTEM.CNF: %s\n",buf);}
 iso_close(&disc); if(iop_cdvd_mount_iso(argv[2]))return 5; iop_cdvd_set_disc_present(0x12); iop_cdrom_legacy_mount_iso(argv[2]); printf("DISC MMIO type=%02x\n",iop_cdvd_get_disc_type());
 ee_state_t *s=ee_core_get_state();gs_state_t *g=gs_get_state();gif_state_t *f=gif_get_state();
 uint64_t budget=(argc>3?strtoull(argv[3],0,10):2000)*1000000ull;
 int logo_saved=0;
 while(s->instructions_executed<budget&&!s->halted&&!iop_core_get_state()->halted){
  iop_sio2_pad_set_buttons(s->instructions_executed>=400000000ull && s->instructions_executed<450000000ull?IOP_PAD_BTN_START:0);
  system_run_interleaved(1000000);
  if(!logo_saved && s->instructions_executed>=695000000ull) {
   uint32_t bp,bw;gs_decode_dispfb(g->dispfb2,&bp,&bw);if(!bw)bw=640;
   FILE *img=fopen("work/tekken-r1297-logo.ppm","wb");
   if(img){fprintf(img,"P6\n640 224\n255\n");for(unsigned y=0;y<224;y++)for(unsigned x=0;x<640;x++){uint32_t pixel=gs_mem_read_psmct32(bp,bw,x,y);uint8_t rgb[3]={pixel,pixel>>8,pixel>>16};fwrite(rgb,1,3,img);}fclose(img);}logo_saved=1;
  }
  uint64_t rc[4],a,c,h,re;uint32_t rl[6],cd,se;int use;int32_t max,cnt,wait;
  ee_core_get_r1249_repair(rc,rl);ee_core_get_r1190_sif_diag(&a,&c,&h,&re,&cd,&se);
  ee_hle_thread_get_sema_state(5,&use,&max,&cnt,&wait);
  printf("BOOT n=%llu pc=%08x tid=%d DISC=%02x REND=%llu repair=%llu S5=%d/%d w=%d sig=%llu pmode=%llx qw=%llu sprite=%llu tri=%llu\n",(unsigned long long)s->instructions_executed,s->pc,ee_hle_thread_get_current_thread_id(),iop_cdvd_get_disc_type(),(unsigned long long)re,(unsigned long long)rc[0],cnt,max,wait,(unsigned long long)ee_hle_thread_get_signal_calls(5),(unsigned long long)g->pmode,(unsigned long long)f->quadwords_seen,(unsigned long long)f->sprites_drawn,(unsigned long long)f->triangles_drawn);
 }
 for(int t=1;t<=ee_hle_thread_get_thread_count();t++)printf("THREAD %d status=%u prio=%u pc=%08x wait=%u/%u\n",t,ee_hle_thread_get_status(t),ee_hle_thread_get_priority(t),ee_hle_thread_get_saved_pc(t),ee_hle_thread_get_wait_type(t),ee_hle_thread_get_wait_id(t));
 ee_intc_state_t *ic=ee_intc_get_state();
 printf("CPU status=%08x cause=%08x EPC=%08x BadVA=%08x INTC=%08x/%08x SP=%08x RA=%08x\n",s->cop0[12],s->cop0[13],s->cop0[14],s->cop0[8],ic->stat,ic->mask,(uint32_t)s->gpr[29].ud0,(uint32_t)s->gpr[31].ud0);
 for(int k=0;k<16;k++)printf("SEMA %d signals=%llu\n",k,(unsigned long long)ee_hle_thread_get_signal_calls(k));
 if(argc>4) printf("CHECKPOINT result=%d\n",checkpoint_save(argv[4]));
 uint32_t bp=(g->dispfb2&511u)*2048u,bw=((g->dispfb2>>9)&63u)*64u;if(!bw)bw=640;
 printf("GS pmode=%llx dispfb2=%llx display2=%llx fbp=%u ctx1=%u\n",(unsigned long long)g->pmode,(unsigned long long)g->dispfb2,(unsigned long long)g->display2,f->fbp,f->ctx1_fbp);
 scan("DISPFB2",bp,bw);scan("GIF",f->fbp,f->fbw?f->fbw:640);
 FILE *image=fopen("work/tekken-gs.ppm","wb");
 if(image){fprintf(image,"P6\n640 448\n255\n");for(unsigned y=0;y<448;y++)for(unsigned x=0;x<640;x++){uint32_t p=gs_mem_read_psmct32(bp,bw,x,y);unsigned char c[3]={(unsigned char)p,(unsigned char)(p>>8),(unsigned char)(p>>16)};fwrite(c,1,3,image);}fclose(image);}
 for(int j=0;j<32;j++)printf("REG %d=%016llx\n",j,(unsigned long long)s->gpr[j].ud0);
 dma_state_t *ds=dma_get_state();printf("DMAC CTRL=%08x RBSR=%08x RBOR=%08x\n",ds->d_ctrl,ds->d_rbsr,ds->d_rbor);for(int j=0;j<10;j++)printf("DMA %d CHCR=%08x MADR=%08x QWC=%u TADR=%08x SADR=%08x\n",j,ds->chan[j].chcr,ds->chan[j].madr,ds->chan[j].qwc,ds->chan[j].tadr,ds->chan[j].sadr);
 printf("SCRATCH:");for(int j=0;j<64;j++)printf(" %02x",s->scratch[j]);puts("");
 for(int plane=0;plane<2;plane++) {
  uint64_t df=plane?g->dispfb2:g->dispfb1,display=plane?g->display2:g->display1;uint32_t pb,pw,sx,sy,sw,sh;gs_decode_dispfb(df,&pb,&pw);gs_decode_display_region(df,display,g->smode2,&sx,&sy,&sw,&sh);if(!pw)pw=640;
  printf("DISPLAY %d PMODE=%llx BP=%u BW=%u XY=%u/%u WH=%u/%u\n",plane+1,(unsigned long long)g->pmode,pb,pw,sx,sy,sw,sh);
  char path[96];snprintf(path,sizeof(path),"work/tekken_r1257_display%d.ppm",plane+1);FILE*img=fopen(path,"wb");if(img){fprintf(img,"P6\n640 448\n255\n");for(unsigned y=0;y<448;y++)for(unsigned x=0;x<640;x++){uint32_t p=gs_mem_read_psmct32(pb,pw,x,y);uint8_t b[3]={p,p>>8,p>>16};fwrite(b,1,3,img);}fclose(img);}
 }
 FILE *dump=fopen("work/tekken_r1256_ram.bin","wb");if(dump){fwrite(s->ram,1,32*1024*1024,dump);fclose(dump);}
 printf("HALT=%d reason=%s\n",s->halted,s->halt_reason);iop_state_t *iop=iop_core_get_state();printf("IOP HALT=%d reason=%s PC=%08x instructions=%llu\n",iop->halted,iop->halt_reason,iop->pc,(unsigned long long)iop->instructions_executed);return (s->halted||iop->halted)?1:0;
}
