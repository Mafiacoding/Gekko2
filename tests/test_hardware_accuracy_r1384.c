#include <stdio.h>
#include <string.h>
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/hw/vu.h"
#include "core/hw/gif.h"
#include "core/hw/vif.h"
#include "core/hw/dma.h"
#define CHECK(x) do{if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t vf[32][4],vi[32],acc[4],pc,bd,bt,ed;
static uint64_t retired,unknown;
static uint8_t mem[16384],micro[16384];
static vu_pipeline_t pipeline;static vu_timing_t timing;
static void put(uint8_t*p,uint32_t v){for(unsigned k=0;k<4;k++)p[k]=v>>(8*k);}
static uint32_t up(unsigned d,unsigned s,unsigned t,unsigned mask,unsigned fn){return mask<<21|t<<16|s<<11|d<<6|fn;}
#define NOP up(11,0,0,0,63)
static void reset(void){memset(vf,0,sizeof vf);memset(vi,0,sizeof vi);memset(acc,0,sizeof acc);memset(&pipeline,0,sizeof pipeline);memset(&timing,0,sizeof timing);pc=bd=bt=ed=0;retired=unknown=0;vf[0][3]=0x3f800000;}
static void step(uint32_t upper,uint32_t lower){put(micro+pc,lower);put(micro+pc+4,upper);vu_micro_step_timed(vf,vi,acc,mem,16383,micro,16383,&pc,&bd,&bt,&ed,&retired,&unknown,&pipeline,&timing);}
static void packet(uint8_t*p,uint32_t prim){memset(p,0,32);put(p,0x8001);put(p+4,0x10000000);put(p+8,0xe);put(p+16,prim);put(p+24,0);}
int main(void){
 /* Independent four-clock flag visibility, including the paired query. */
 reset();vf[1][0]=0x3f800000;vf[2][0]=0xbf800000;vi[16]=0x20;vi[17]=0xaaaa;
 step(up(3,1,2,8,40),(22u<<25)|(4u<<16)|15u);
 CHECK(vf[3][0]==0&&vi[4]==0&&vi[16]==0x20&&vi[17]==0xaaaa);
 for(unsigned n=0;n<3;n++)step(NOP,0);
 CHECK(vi[17]==0xaaaa);step(NOP,(22u<<25)|(4u<<16)|15u);
 CHECK(vi[4]==1&&vi[17]==8&&(vi[16]&0x3f)==0x21&&(vi[16]&0x40));
 /* Full lane Z/S/U/O masks and VF00 destinations still set flags. */
 reset();vf[1][0]=0;vf[1][1]=0xbf800000;vf[1][2]=0x00800000;vf[1][3]=0x7f7fffff;
 for(unsigned n=0;n<4;n++)vf[2][n]=0x3f000000;
 vf[2][3]=0x40000000;
 step(up(0,1,2,15,42),0);for(unsigned n=0;n<4;n++)step(NOP,0);
 CHECK(vi[17]==(8u|2u|4u<<4|2u<<8|1u<<12));CHECK((vi[16]&15)==15);
 CHECK(vf[0][3]==0x3f800000);
 /* Simultaneous FMAC + FDIV preserves visible flags until each matures. */
 reset();vi[16]=0x40;vf[1][0]=0x3f800000;vf[2][0]=0xbf800000;
 uint32_t div=0x80000000u|(2u<<16)|(1u<<11)|(14u<<6)|60u;
 step(up(3,1,2,8,40),div);CHECK(vi[16]==0x40);
 for(unsigned n=0;n<4;n++)step(NOP,0);CHECK((vi[16]&1)==1);
 step(NOP,div|3u);CHECK((vi[16]&1)==1&&vi[22]==0xbf800000);
 /* FSSET retires at four cycles and preserves FDIV current I/D. */
 reset();vi[16]=0x31;vi[17]=0x1234;
 step(NOP,(21u<<25)|0x100);CHECK(vi[16]==0x31);
 for(unsigned n=0;n<4;n++)step(NOP,0);CHECK(vi[16]==0x131&&vi[17]==0x1234);

 /* FSSET snapshots issued FMAC flags, not the older visible MAC. */
 reset();vf[1][0]=0x3f800000;vf[2][0]=0xbf800000;
 step(up(3,1,2,8,40),0);step(NOP,(21u<<25)|0x100);
 for(unsigned n=0;n<4;n++)step(NOP,0);CHECK(vi[17]==8&&(vi[16]&0xfcf)==0x101);
 /* At a common ready clock FMAC/FSSET publishes before FDIV. */
 reset();vf[1][0]=0x3f800000;vf[2][0]=0;
 step(NOP,div);step(NOP,0);step(NOP,0);step(NOP,21u<<25);
 step(NOP,div|3u);CHECK((vi[16]&0xc30)==0x820);
 /* Production M boundary remains RUN, advances on common EE retirement. */
 bios_image_t b={0};CHECK(system_init(&b,&b)==0);ee_state_t*s=ee_core_get_state();
 vu0_micro_write32(s,0,0);vu0_micro_write32(s,4,NOP|0x20000000u);
 vu0_micro_write32(s,8,0);vu0_micro_write32(s,12,NOP|0x40000000u);
 vu0_micro_write32(s,16,0);vu0_micro_write32(s,20,NOP);
 vu0_exec_micro(s,0);CHECK(s->vu0_running&&s->cop2_ctrl[26]==8&&(s->cop2_ctrl[29]&1));
 s->pc=0x80100000;s->next_pc=s->pc+4;ee_mem_write32(s,s->pc,0);
 CHECK(!ee_core_step());CHECK(s->vu0_running&&s->cop2_ctrl[26]==8);
 ee_mem_write32(s,s->pc,0);CHECK(!ee_core_step());CHECK(s->cop2_ctrl[26]==16);
 ee_mem_write32(s,s->pc,0);CHECK(!ee_core_step());CHECK(!s->vu0_running&&!(s->cop2_ctrl[29]&1));
 /* Arbitration retains ownership until EOP and respects both masks. */
 uint8_t a[32],c[32];packet(a,0x46);packet(c,0x20);gif_init();
 CHECK(gif_try_process_quadwords(GIF_PATH_3,a,1)==1);
 CHECK(gif_try_process_quadwords(GIF_PATH_1,c,2)==0);
 uint32_t stat;gif_mmio_read32(GIF_STAT_ADDR,&stat);CHECK((stat>>10&3)==3&&(stat&1u<<8));
 CHECK(gif_try_process_quadwords(GIF_PATH_3,a+16,1)==1);
 CHECK(gif_try_process_quadwords(GIF_PATH_1,c,2)==2);CHECK(gif_get_state()->prim==0x20);
 gif_mmio_write32(GIF_MODE_ADDR,1);CHECK(gif_try_process_quadwords(GIF_PATH_3,a,2)==0);
 gif_mmio_write32(GIF_MODE_ADDR,0);gif_set_path3_mask(1);CHECK(gif_try_process_quadwords(GIF_PATH_3,a,2)==0);
 gif_set_path3_mask(0);CHECK(gif_try_process_quadwords(GIF_PATH_3,a,2)==2);
 /* DMA pause/mask holds STR, QWC, MADR, completion and payload. */
 gif_init();dma_init();dma_bind_ee_ram(s->ram,s->ram_size);dma_set_sink(DMA_CHANNEL_GIF,gif_process_quadwords);
 memcpy(s->ram+0x1000,a,32);dma_channel_t*d=&dma_get_state()->chan[DMA_CHANNEL_GIF];
 d->madr=0x1000;d->qwc=2;d->chcr=0x100;
 gif_mmio_write32(GIF_CTRL_ADDR,8);dma_channel_kick(DMA_CHANNEL_GIF);
 CHECK(d->qwc==2&&d->madr==0x1000&&(d->chcr&0x100)&&!(dma_get_state()->d_stat&4));
 gif_mmio_write32(GIF_CTRL_ADDR,0);CHECK(dma_gif_service());
 CHECK(d->qwc==0&&d->madr==0x1020&&!(d->chcr&0x100)&&(dma_get_state()->d_stat&4));
 /* Resumable END chain payload: tag is not reread after a stall. */
 dma_init();dma_bind_ee_ram(s->ram,s->ram_size);dma_set_sink(DMA_CHANNEL_GIF,gif_process_quadwords);gif_init();
 put(s->ram+0x2000,0x70000002);put(s->ram+0x2004,0);memcpy(s->ram+0x2010,a,32);
 d=&dma_get_state()->chan[DMA_CHANNEL_GIF];d->tadr=0x2000;d->chcr=0x104;
 gif_set_path3_mask(1);dma_channel_kick(DMA_CHANNEL_GIF);CHECK(d->qwc==2&&d->madr==0x2010&&(d->chcr&0x100));
 put(s->ram+0x2000,0xffffffff);gif_set_path3_mask(0);CHECK(dma_gif_service());
 CHECK(d->madr==0x2030&&d->qwc==0&&d->tadr==0x2030&&!(d->chcr&0x100));

 /* IMT interrupts PATH3 IMAGE, then restores its unfinished ownership. */
 gif_init();uint8_t image[48]={0};put(image,0x8002);put(image+4,0x08000000);
 CHECK(gif_try_process_quadwords(GIF_PATH_3,image,1)==1);
 CHECK(gif_try_process_quadwords(GIF_PATH_1,c,2)==0);
 gif_mmio_write32(GIF_MODE_ADDR,4);CHECK(gif_try_process_quadwords(GIF_PATH_1,c,2)==2);
 CHECK(gif_get_state()->arb_owner==3);CHECK(gif_try_process_quadwords(GIF_PATH_3,image+16,2)==2);
 /* DIRECTHL retains payload and trailing VIFcodes while PATH3 is active. */
 gif_init();vif_init();gif_mmio_write32(GIF_MODE_ADDR,4);
 CHECK(gif_try_process_quadwords(GIF_PATH_3,image,1)==1);
 uint8_t direct[48]={0};put(direct,0x51000002);memcpy(direct+4,c,32);
 vif1_process_quadwords(DMA_CHANNEL_VIF1,direct,3);
 CHECK(vif1_get_state()->direct_needed_words==8&&vif1_get_state()->direct_sent_words==0);
 CHECK(vif1_get_state()->stall_words==3&&gif_get_state()->prim!=0x20);
 CHECK(gif_try_process_quadwords(GIF_PATH_3,image+16,2)==2);vif_service();
 CHECK(vif1_get_state()->direct_needed_words==0&&vif1_get_state()->stall_words==0&&gif_get_state()->prim==0x20);
 /* REFS stall at STADR preserves an in-progress packet and its QWC. */
 dma_init();gif_init();dma_bind_ee_ram(s->ram,s->ram_size);dma_set_sink(DMA_CHANNEL_GIF,gif_process_quadwords);
 put(s->ram+0x3000,0x40000002);put(s->ram+0x3004,0x1000);
 put(s->ram+0x3010,0x70000000);put(s->ram+0x3014,0);
 d=&dma_get_state()->chan[DMA_CHANNEL_GIF];d->tadr=0x3000;d->chcr=0x104;
 dma_get_state()->d_ctrl=3u<<6;dma_get_state()->d_stadr=0x1000;
 dma_channel_kick(DMA_CHANNEL_GIF);CHECK(d->qwc==2&&(d->chcr&0x100)&&(dma_get_state()->d_stat&(1u<<13)));
 dma_get_state()->d_stadr=0x1010;CHECK(dma_gif_service());CHECK(d->qwc==1&&d->madr==0x1010);
 dma_get_state()->d_stadr=0x1020;CHECK(dma_gif_service());CHECK(!d->qwc&&!(d->chcr&0x100));
 /* CALL/RET consumes the stored DMA address stack exactly once. */
 dma_init();gif_init();dma_bind_ee_ram(s->ram,s->ram_size);dma_set_sink(DMA_CHANNEL_GIF,gif_process_quadwords);
 put(s->ram+0x3000,0x50000000);put(s->ram+0x3004,0x3100);
 put(s->ram+0x3010,0x70000000);put(s->ram+0x3014,0);
 put(s->ram+0x3100,0x60000002);put(s->ram+0x3104,0);memcpy(s->ram+0x3110,a,32);
 d=&dma_get_state()->chan[DMA_CHANNEL_GIF];d->tadr=0x3000;d->chcr=0x104;
 dma_channel_kick(DMA_CHANNEL_GIF);CHECK(!(d->chcr&0x130)&&d->asr0==0x3010&&d->quadwords_transferred==2);

 /* CPU FIFO depth/backpressure and FQC, including precise SQ retry. */
 gif_init();gif_mmio_write32(GIF_CTRL_ADDR,8);
 for(unsigned n=0;n<16;n++)CHECK(gif_fifo_write((n&1)?a+16:a));
 CHECK(!gif_fifo_write(c));gif_mmio_read32(GIF_STAT_ADDR,&stat);CHECK((stat>>24&31)==16);
 s->vu0_running=0;s->vu0_timing.m_yield=0;s->pc=0x80101000;s->next_pc=s->pc+4;
 s->gpr[1].ud0=0x10006000;s->gpr[2].ud0=0;s->gpr[2].ud1=0;
 ee_mem_write32(s,s->pc,(0x1fu<<26)|(1u<<21)|(2u<<16));uint64_t before=s->instructions_executed;
 CHECK(!ee_core_step());CHECK(s->pc==0x80101000&&s->instructions_executed==before&&gif_get_state()->fifo_count==16);
 gif_mmio_write32(GIF_CTRL_ADDR,0);gif_fifo_service();CHECK(!gif_get_state()->fifo_count);
 CHECK(!ee_core_step());CHECK(s->pc==0x80101004&&s->instructions_executed==before+1);
 /* Host safety cap never invents a VU0 stop. */
 memset(s->vu0_micro,0,sizeof s->vu0_micro);memset(&s->vu0_timing,0,sizeof s->vu0_timing);
 s->vu0_branch_delay=s->vu0_ebit_delay=0;vu0_exec_micro(s,0);
 CHECK(s->vu0_running&&(s->cop2_ctrl[29]&1));
 puts("PASS R1384 FMAC visibility/lane flags/FDIV/FSSET, M retirement, GIF arbitration and resumable DMA");return 0;
}
