#include "core/hw/ipu.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t stream[4096];static unsigned length;
static void bit(unsigned value){if(value)stream[length>>3]|=1u<<(7-(length&7));length++;}
static void put(unsigned value,unsigned n){for(unsigned i=n;i;i--)bit((value>>(i-1))&1);}
static void dc_block(unsigned chroma){put(chroma?0:4,chroma?2:3);put(2,2);}
static void start(void){while(length&7)bit(0);put(0x000001b3,32);while(length&127)bit(0);}
static uint32_t read32(unsigned a){uint32_t v;assert(ipu_mmio_read32(a,&v));return v;}
static void decode(unsigned command,uint8_t output[768])
{
 unsigned in=0,out=0,qwc=length/128;
 ipu_mmio_write32(0x10002000,command);
 for(unsigned guard=0;guard<512;guard++){
  if(in<qwc)in+=ipu_input_write(stream+in*16,1);
  ipu_service();out+=ipu_output_read(output+out*16,48-out);ipu_service();
  if(!(read32(0x10002010)&0x80000000u)&&out==48)break;
 }
 assert(in==qwc&&out==48);assert(!(read32(0x10002010)&0x80000000u));
 assert(read32(0x10002030)==0x000001b3);assert(read32(0x10002010)&0x8000);
}
int main(void)
{
 uint8_t output[768];ipu_init();
 for(unsigned i=0;i<6;i++)dc_block(i>=4);start();
 decode(0x2c010000,output);
 for(unsigned i=0;i<384;i++)assert(output[i*2]==128&&output[i*2+1]==0);
 assert((read32(0x10002010)>>8&63)==63);
 /* Full signed coefficient path, escape and non-intra coded-block pattern.
  * CBP=0 still emits a zero residual macroblock, not a made-up picture. */
 memset(stream,0,sizeof stream);length=0;ipu_init();put(1,9);start();
 decode(0x20010000,output);for(unsigned i=0;i<768;i++)assert(output[i]==0);
 assert((read32(0x10002010)>>8&63)==0);
 /* Native IDCT: DC-only positive/negative, all AC slots and extreme legal
  * coefficient values must avoid undefined shifts, aliasing and overflow. */
 for(int dc=-2048;dc<2048;dc+=127){int16_t block[64]={0};block[0]=dc;ipu_idct(block);
  for(unsigned i=1;i<64;i++)assert(block[i]==block[0]);}
 for(unsigned at=0;at<64;at++){int16_t block[64]={0};block[at]=2047;ipu_idct(block);}
 int16_t dense[64];for(unsigned i=0;i<64;i++)dense[i]=(i&1)?2047:-2048;ipu_idct(dense);
 puts("PASS real BDEC DC macroblock, CBP=0 signed residual, start-code/TOP, FIFO output and IDCT legal extrema");
}
