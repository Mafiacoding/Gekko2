/* Synthetic streams only: no BIOS/disc data. */
#include "core/hw/ipu.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static void expect(uint32_t bits,unsigned table,uint32_t ctrl,uint32_t data,unsigned used)
{unsigned n=99;assert(ipu_vlc_decode(bits,table,ctrl,&n)==data);assert(n==used);}
static uint32_t read32(uint32_t a){uint32_t v;assert(ipu_mmio_read32(a,&v));return v;}
int main(void)
{
 expect(0x80000000,0,0,0x10001,1);expect(0x60000000,0,0,0x30002,3);
 expect(8u<<21,0,0,0xb0023,11);expect(15u<<21,0,1u<<23,0xb0022,11);
 expect(15u<<21,0,0,0,0);expect(0,0,0,0,0);
 expect(0x80000000,1,1u<<24,1,1);expect(0x40000000,1,1u<<24,17,2);
 expect(0x80000000,1,2u<<24,138,1);expect(0x80000000,1,3u<<24,0x2008c,2);
 expect(0x80000000,2,0,0x10000,1);expect(0x40000000,2,0,0x20001,3);
 expect(0x60000000,2,0,0xffffffff,3);
 expect(0,3,0,0x10000,1);expect(0x80000000,3,0,0x20001,2);expect(0xc0000000,3,0,0xffffffff,2);
 /* Full table input domains are bounds-checked by ASan/UBSan. */
 for(unsigned t=0;t<4;t++)for(unsigned p=0;p<8;p++)for(unsigned x=0;x<65536;x++){
  unsigned n;ipu_vlc_decode(x<<16,t,p<<24,&n);assert(n<=11);
 }
 /* Decode succeeds at the end of a QWC, then TOP starves. Refill must
  * resume TOP without consuming the VLC twice, including checkpoint restore. */
 ipu_init();ipu_mmio_write32(0x10002000,96);uint8_t q[16]={0};q[12]=0x80;
 assert(ipu_input_write(q,1)==1);ipu_mmio_write32(0x10002000,0x30000000);
 assert(read32(0x10002000)==0x10001);assert(read32(0x10002004)==0x80000000);
 assert((read32(0x10002020)&127)==97);
 ipu_state_t saved=*ipu_get_state();ipu_init();ipu_restore(&saved);
 memset(q,0,16);assert(ipu_input_write(q,1)==1);ipu_service();
 assert(read32(0x10002004)==0);assert((read32(0x10002020)&127)==97);
 /* Fast byte extraction agrees with an independent per-bit oracle at
  * every possible QWC bit offset, including the second internal QWC. */
 uint8_t stream[32];for(unsigned i=0;i<32;i++)stream[i]=(i*73+11)&255;
 for(unsigned bp=0;bp<128;bp++){
  ipu_init();ipu_mmio_write32(0x10002000,bp);assert(ipu_input_write(stream,2)==2);
  ipu_mmio_write32(0x10002000,0x40000000);uint32_t v=0;
  for(unsigned j=0;j<32;j++)v=(v<<1)|((stream[(bp+j)>>3]>>(7-((bp+j)&7)))&1);
  assert(read32(0x10002000)==v);
 }
 uint8_t rgba[1024],out[512],palette[32]={0};
 for(unsigned i=0;i<256;i++){rgba[i*4]=248;rgba[i*4+1]=0;rgba[i*4+2]=0;rgba[i*4+3]=0x40;}
 ipu_pack_convert(rgba,out,1,0,palette);for(unsigned i=0;i<256;i++){assert(out[i*2]==31);assert(out[i*2+1]==128);}
 palette[6]=31;ipu_pack_convert(rgba,out,0,0,palette);for(unsigned i=0;i<128;i++)assert(out[i]==0x33);
 /* Real PACK consumes 64 QWCs in FIFO-sized pieces, stalls on output and
  * emits exactly 32 QWCs. No canned RGB/guest progression is supplied. */
 ipu_init();ipu_mmio_write32(0x10002000,0x88000001);
 unsigned in=0,used=0;uint8_t result[512];
 for(unsigned guard=0;guard<100;guard++){
  if(in<64){unsigned n=ipu_input_write(rgba+in*16,64-in>8?8:64-in);in+=n;}
  ipu_service();used+=ipu_output_read(result+used*16,32-used);ipu_service();
  if(!(read32(0x10002010)&0x80000000u)&&used==32)break;
 }
 assert(in==64&&used==32);ipu_pack_convert(rgba,out,1,0,palette);assert(!memcmp(result,out,512));
 puts("PASS VDEC tables/sign/escape, resumable TOP/checkpoint, all bit offsets, PACK RGB16/VQ and FIFO backpressure");
}
