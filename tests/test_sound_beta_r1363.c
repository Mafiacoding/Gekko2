#include "core/hw/audio_output.h"
#include "core/hw/iop_spu2.h"
#include "core/hw/spu2_mixer.h"
#include "core/hw/iop_dma.h"
#include "core/hw/iop_intc.h"
#include "core/iop/iop_core.h"
#include "core/bios_loader.h"
#include "core/recompiler/optimization.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void wr(int core,unsigned reg,unsigned value){assert(iop_spu2_mmio_write16(iop_spu2_core_reg_addr(core,reg),value));}
static unsigned rd(int core,unsigned reg){uint16_t v;assert(iop_spu2_mmio_read16(iop_spu2_core_reg_addr(core,reg),&v));return v;}
static void dma(int core,unsigned madr,unsigned words,unsigned direction){unsigned base=core?0x1f801500u:0x1f8010c0u;
 assert(iop_dma_mmio_write32(base,madr));assert(iop_dma_mmio_write32(base+4,0x10000u|words));assert(iop_dma_mmio_write32(base+8,0x01000200u|direction));}
static unsigned chcr(int core){uint32_t v;assert(iop_dma_mmio_read32((core?0x1f801500u:0x1f8010c0u)+8,&v));return v;}
static void reset(void){iop_core_get_state()->sched_ticks=0;iop_spu2_init();iop_dma_init();iop_intc_init();}
int main(void){
 static uint8_t rom[4*1024*1024];bios_image_t bios={0};bios.data=rom;bios.size=sizeof rom;
 assert(!iop_core_init(&bios));iop_state_t *iop=iop_core_get_state();
 gekko2_optimization_mask|=GEKKO2_OPT_BIT(GEKKO2_OPT_SOUND);
 /* DMA core selection and 16-bit-word address units, including reverse transfer. */
 reset();for(unsigned i=0;i<16;i++)iop->ram[0x100+i]=0x80+i;
 wr(0,SPU2_C_TSA_LO,0x4000);dma(0,0x100,4,1);
 assert(!memcmp(spu2_mixer_get_ram()+0x8000,iop->ram+0x100,16));assert(rd(0,SPU2_C_TSA_LO)==0x4008);
 assert(!(chcr(0)&0x1000000u)&&iop_dma_get_spu2_transfer_count()==1);
 wr(1,SPU2_C_TSA_LO,0x5000);dma(1,0x100,4,1);
 assert(!memcmp(spu2_mixer_get_ram()+0xa000,iop->ram+0x100,16));assert(rd(1,SPU2_C_TSA_LO)==0x5008);
 memset(iop->ram+0x200,0,16);wr(1,SPU2_C_TSA_LO,0x5000);dma(1,0x200,4,0);assert(!memcmp(iop->ram+0x100,iop->ram+0x200,16));
 /* Wrap at the actual 20-bit SPU word address and FIFO data delivery. */
 wr(0,SPU2_C_TSA_HI,15);wr(0,SPU2_C_TSA_LO,0xfffe);dma(0,0x100,4,1);
 assert(!memcmp(spu2_mixer_get_ram()+0x1ffffc,iop->ram+0x100,4));assert(!memcmp(spu2_mixer_get_ram(),iop->ram+0x104,12));
 wr(0,SPU2_C_TSA_HI,0);wr(0,SPU2_C_TSA_LO,0x6000);wr(0,SPU2_C_TDATA,0x1234);
 assert(spu2_mixer_get_ram()[0xc000]==0x34&&spu2_mixer_get_ram()[0xc001]==0x12&&rd(0,SPU2_C_TSA_LO)==0x6001);
 wr(0,SPU2_C_TSA_LO,0x6000);assert(rd(0,SPU2_C_TDATA)==0x1234&&rd(0,SPU2_C_TSA_LO)==0x6001);
 /* Memory IRQ fires once, disabling IRQ clears the SPU-side latch. */
 wr(1,SPU2_C_IRQA_LO,0x6000);wr(1,SPU2_C_CTRL,0x40);wr(0,SPU2_C_TSA_LO,0x6000);wr(0,SPU2_C_TDATA,0xabcd);
 assert(iop_intc_get_state()->istat&(1u<<9));assert(iop_spu2_audio_stat(1)==1);iop_spu2_check_ram_irq(0x6000,1);assert(iop_spu2_audio_stat(1)==1);
 wr(1,SPU2_C_CTRL,0);uint16_t info;assert(iop_spu2_mmio_read16(IOP_SPU2_BASE+SPU2_S_IRQINFO,&info)&&!(info&8));
 uint16_t v16;uint32_t v32;
 assert(!iop_spu2_mmio_read16(IOP_SPU2_BASE+0x7ff,&v16));assert(!iop_spu2_mmio_read32(IOP_SPU2_BASE+0x7fe,&v32));
 assert(!iop_spu2_mmio_write32(IOP_SPU2_BASE+0x7fe,~0u));
 puts("PASS DMA4/7 core mapping, word units, reverse copy, wrap, FIFO, IRQ latch and MMIO bounds");
 /* Parked IOP catch-up must stop before an armed SPU sample IRQ. */
 reset();iop->idle=1;iop->branch_delay_pending=0;iop->load_delay_reg=0;iop->exception_pending=0;
 /* Even silent core0 writes its output ring at word 0x800. */
 wr(0,SPU2_C_IRQA_LO,0x800);wr(0,SPU2_C_CTRL,0x40);
 gekko2_optimization_mask|=GEKKO2_OPT_BIT(GEKKO2_OPT_IOP_IDLE_BATCH);
 assert(iop_core_step_n(767)==767&&!(iop_intc_get_state()->istat&(1u<<9)));
 assert(iop_core_step_n(1)==1&&(iop_intc_get_state()->istat&(1u<<9))&&iop_spu2_audio_stat(0)==1);
 puts("PASS idle batch preserves silent mixer-output IRQ at the first 768-cycle sample boundary");
 /* Native split stereo AutoDMA: three 256-frame chunks; real completion
  * follows consumption of the last frame, not merely staging the data. */
 reset();wr(1,SPU2_C_ADMAS,2);wr(1,SPU2_C_MMIX,0xc0);wr(1,SPU2_C_CTRL,0x8000);
 assert(iop_spu2_mmio_write16(IOP_SPU2_BASE+SPU2_S_MVOLL+0x28,0x3fff));assert(iop_spu2_mmio_write16(IOP_SPU2_BASE+SPU2_S_MVOLR+0x28,0x3fff));
 assert(iop_spu2_mmio_write16(IOP_SPU2_BASE+SPU2_S_INPVOLL+0x28,0x7fff));assert(iop_spu2_mmio_write16(IOP_SPU2_BASE+SPU2_S_INPVOLR+0x28,0x7fff));
 for(unsigned chunk=0;chunk<3;chunk++)for(unsigned i=0;i<256;i++){
  int16_t l=(chunk+1)*1000,r=-l;unsigned p=0x1000+chunk*1024+i*2;
  iop->ram[p]=(uint8_t)l;iop->ram[p+1]=(uint8_t)(l>>8);iop->ram[p+512]=(uint8_t)r;iop->ram[p+513]=(uint8_t)(r>>8);
 }
 dma(1,0x1000,768,1);assert(chcr(1)&0x1000000);assert(rd(1,SPU2_C_STATX)&0x400u);assert(iop_dma_get_spu2_transfer_count()==0);
 iop->sched_ticks=767; iop_spu2_sync(iop->sched_ticks);assert(!iop_spu2_audio_stat(0));
 iop->sched_ticks=767u*768u;iop_spu2_sync(iop->sched_ticks);assert(chcr(1)&0x1000000);
 iop->sched_ticks=768u*768u;iop_spu2_sync(iop->sched_ticks);assert(!(chcr(1)&0x1000000));assert(iop_dma_get_spu2_transfer_count()==1);assert(!(rd(1,SPU2_C_STATX)&0x400u));
 int16_t out[768*2];audio_output_consume(out,768);
 for(unsigned i=0;i<768;i++){int expected=(i/256+1)*1000;assert(abs(out[i*2]-expected)<4&&abs(out[i*2+1]+expected)<4);}
 assert(spu2_mixer_get_total_frames_rendered()==768&&iop_spu2_audio_stat(2)==1);
 audio_output_info ai;audio_output_get_info(&ai);assert(ai.submitted==768&&ai.nonzero==768&&ai.played==768&&ai.queued==0);
 int16_t silence[32];memset(silence,0x55,sizeof silence);audio_output_consume(silence,16);for(unsigned i=0;i<32;i++)assert(!silence[i]);
 assert(iop_spu2_adma_begin(1,0x1000,100)==-1); /* returns -1 for unsupported partial block */
 puts("PASS guest-clock 48 kHz, split-stereo ADMA data, consumption-timed DMA completion and nonblocking output");
 /* Checkpoint of the core preserves counters and does not preserve host PCM. */
 void *saved=malloc(iop_spu2_snapshot_size());assert(saved);iop_spu2_snapshot_save(saved);
 assert(iop_spu2_snapshot_valid(saved,iop->sched_ticks,iop->ram_size));iop_spu2_init();iop_spu2_snapshot_load(saved);
 assert(iop_spu2_audio_stat(0)==768);free(saved);audio_output_get_info(&ai);assert(!ai.queued&&!ai.running);
 audio_output_reset();int16_t *many=malloc(8400u*4u);assert(many);for(unsigned k=0;k<8400u*2u;k++)many[k]=1234;
 audio_output_submit(many,8400);audio_output_get_info(&ai);assert(ai.queued==8192&&ai.dropped==208);
 audio_output_consume(many,8400);for(unsigned k=0;k<8192u*2u;k++)assert(many[k]==1234);
 for(unsigned k=8192u*2u;k<8400u*2u;k++)assert(!many[k]);free(many);audio_output_start();audio_output_stop();audio_output_get_info(&ai);assert(!ai.queued&&!ai.running);
 puts("PASS bounded PCM queue overflow, zero-fill underflow and pause/stop flush");
 iop_core_shutdown();puts("PASS sound snapshot state validation and empty host queue after restore");return 0;
}
