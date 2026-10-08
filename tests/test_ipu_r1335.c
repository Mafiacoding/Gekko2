#include "core/hw/ipu.h"
#include "core/hw/dma.h"
#include "core/hw/ee_intc.h"
#include "core/hw/arm_worker.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t ram[8192];static unsigned writes;
static void notify(uint32_t addr,uint32_t len){assert(addr>=2048&&addr+len<=8192);writes+=len;}
static void put32(unsigned off,uint32_t v){for(unsigned i=0;i<4;i++)ram[off+i]=v>>(i*8);}
static void start(unsigned base,unsigned addr,unsigned qwc,unsigned chcr)
{dma_mmio_write32(base+16,addr);dma_mmio_write32(base+32,qwc);dma_mmio_write32(base,chcr);}
static void reset(void){dma_init();dma_bind_ee_ram(ram,sizeof ram);ipu_init();ee_intc_init();memset(ram,0,sizeof ram);writes=0;}
static void wire(uint8_t *p,uint32_t v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
int main(void)
{
 uint32_t v;uint8_t expected[1024];ipu_profile_t profile;
 reset();ipu_mmio_write32(0x10002000,0x0000007f);ipu_mmio_read32(0x10002020,&v);assert(v==127);
 assert(ee_intc_get_raise_count(8)==1);ipu_mmio_write32(0x10002020,0);ipu_mmio_read32(0x10002020,&v);assert(v==127);
 reset();for(unsigned i=0;i<16;i++)ram[i]=(uint8_t)(i*17);
 ipu_mmio_write32(0x10002000,0x40000004);ipu_mmio_read32(0x10002004,&v);assert(v==0x80000000);
 start(0x1000b400,0,1,0x101);ipu_mmio_read32(0x10002000,&v);assert(v==0x01122334);
 assert(!dma_get_state()->chan[4].qwc&&!(dma_get_state()->chan[4].chcr&0x100));
 reset();memset(ram,0,16);memset(ram+16,255,16);
 ipu_mmio_write32(0x10002000,127);start(0x1000b400,0,2,0x101);
 ipu_mmio_write32(0x10002000,0x40000000);ipu_mmio_read32(0x10002000,&v);assert(v==0x7fffffff);
 /* Matrix loading resumes after starvation without replaying the first half. */
 reset();for(unsigned i=0;i<64;i++)ram[i]=(uint8_t)(i+1);
 ipu_mmio_write32(0x10002000,0x50000000);start(0x1000b400,0,2,0x101);
 assert(ipu_get_state()->busy&&ipu_get_state()->pos==32);
 start(0x1000b400,32,2,0x101);assert(!ipu_get_state()->busy&&!memcmp(ipu_get_state()->iq[0],ram,64));
 reset();start(0x1000b400,0,24,0x101);assert(dma_get_state()->chan[4].qwc==16&&dma_get_state()->chan[4].madr==128);
 assert(dma_get_state()->chan[4].chcr&0x100);assert(!(dma_get_state()->d_stat&(1<<4)));
 reset();for(unsigned i=0;i<64;i++)ram[i]=(uint8_t)i;
 ipu_mmio_write32(0x10002000,0x50000000);start(0x1000b400,0,4,0x101);
 assert(!memcmp(ipu_get_state()->iq[0],ram,64));assert(!ipu_get_state()->busy);
 reset();dma_set_ee_write_notify(notify);
 for(unsigned i=0;i<256;i++)ram[i]=(uint8_t)i;
 memset(ram+256,128,128);
 ipu_csc_convert(ram,expected,0,0,0,0);
 ipu_mmio_write32(0x10002000,0x70000001);
 start(0x1000b000,2048,64,0x100);assert(dma_get_state()->chan[3].qwc==64);
 start(0x1000b400,0,24,0x101);
 assert(!memcmp(ram+2048,expected,1024)&&writes==1024);
 assert(!ipu_get_state()->busy&&!ipu_get_state()->out_count);
 assert(!dma_get_state()->chan[3].qwc&&!(dma_get_state()->chan[3].chcr&0x100));
 assert((dma_get_state()->d_stat&0x18)==0x18);ipu_get_profile(&profile);
 assert(profile.accepted_qwc==24&&profile.output_qwc==64&&profile.csc_macroblocks==1&&!profile.discarded_qwc);
 /* Correct integer limited-range YUV, threshold-zero pixels and RGBA16 alpha. */
 assert(expected[0]==0&&expected[3]==128&&expected[255*4]==255);
 memset(ram,16,256);memset(ram+256,128,128);ipu_csc_convert(ram,expected,0,0,1,2);
 assert(!memcmp(expected,(uint8_t[4]){0},4));ipu_csc_convert(ram,expected,1,0,0,1);assert(expected[0]==0&&expected[1]==128);
 /* Non-neutral primary-colour vectors from the integer reference. */
 const uint8_t samples[3][3]={{81,90,240},{145,54,34},{41,240,110}};
 const uint8_t pixels[3][4]={{254,0,0,128},{0,255,1,128},{0,0,255,128}};
 for(unsigned n=0;n<3;n++){
  memset(ram,samples[n][0],256);memset(ram+256,samples[n][1],64);memset(ram+320,samples[n][2],64);
  ipu_csc_convert(ram,expected,0,0,0,0);assert(!memcmp(expected,pixels[n],4));
 }
 /* A 2-macroblock END chain stalls/resumes without replaying payload. */
 reset();dma_set_ee_write_notify(notify);memset(ram+16,80,512);memset(ram+16+256,128,128);memset(ram+400,128,384);
 put32(0,0x70000030);put32(4,0);dma_mmio_write32(0x1000b430,0);
 ipu_mmio_write32(0x10002000,0x78000002);start(0x1000b400,0,0,0x105);
 assert(dma_get_state()->chan[4].chcr&0x100);
 start(0x1000b000,2048,64,0x100);assert(!ipu_get_state()->busy&&dma_get_state()->chan[4].quadwords_transferred==48);
 assert(dma_get_state()->chan[3].quadwords_transferred==64&&writes==1024);
 /* Snapshots preserve a partial bitstream and bounded state validation. */
 ipu_state_t saved=*ipu_get_state();saved.bp=7;assert(ipu_state_valid(&saved));ipu_restore(&saved);assert(ipu_get_state()->bp==7);
 saved.fp=3;assert(!ipu_state_valid(&saved));
 /* Worker validates ABI/length before writing and matches portable CPU output. */
 uint8_t h[32]={0},output[1024];wire(h,ARM_WORKER_MAGIC);wire(h+4,1);wire(h+16,1);
 memset(ram,100,256);memset(ram+256,128,128);ipu_csc_convert(ram,expected,0,0,0,0);
 assert(gekko2_arm_dispatch(1,h,32,ram,384,output,1024)==1024&&!memcmp(output,expected,1024));
 memset(output,0x7b,sizeof output);assert(gekko2_arm_dispatch(1,h,32,ram,383,output,1024)<0&&output[0]==0x7b);
 wire(h+24,512);assert(gekko2_arm_dispatch(1,h,32,ram,384,output,1024)<0);
 assert(gekko2_arm_dispatch(0,0,0,0,0,h,32)==32&&h[0]=='G');
 puts("PASS IPU bitstream, FIFO backpressure, real FROM/TO DMA, chain resume, CSC formats/alpha, IRQ, state and worker ABI");
}
