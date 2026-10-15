#include "core/hw/ipu.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned length;static uint8_t stream[256];
static void put(unsigned v,unsigned n){for(unsigned i=n;i;i--){if((v>>(i-1))&1)stream[length>>3]|=1u<<(7-(length&7));length++;}}
static void blocks(void){for(unsigned i=0;i<6;i++){put(i<4?4:0,i<4?3:2);put(2,2);}}
int main(void)
{
 for(unsigned format=0;format<2;format++)for(unsigned sign=0;sign<2;sign++){
  memset(stream,0,sizeof stream);length=0;ipu_init();
  put(1,1);blocks();put(1,1);put(1,1);blocks();
  while(length&7)put(0,1);put(0x000001b3,32);while(length&127)put(0,1);
  uint32_t command=0x10010000|(format<<27)|(sign<<25);
  ipu_mmio_write32(0x10002000,command);
  unsigned in=0,out=0,total=format?1024:2048;uint8_t actual[2048],expect[1024],yuv[384];
  memset(yuv,128,sizeof yuv);ipu_csc_convert(yuv,expect,0,0,0,0);
  if(sign)for(unsigned i=0;i<256;i++)for(unsigned c=0;c<3;c++)expect[i*4+c]^=128;
  if(format)ipu_pack_convert(expect,expect,1,0,ipu_get_state()->vq);
  for(unsigned g=0;g<512;g++){
   if(in<length/128)in+=ipu_input_write(stream+in*16,1);
   ipu_service();
   /* Checkpoint mid-output must not restart IDCT or duplicate a macroblock. */
   if(g==1){ipu_state_t saved=*ipu_get_state();assert(ipu_state_valid(&saved));ipu_restore(&saved);}
   if(out<total/16)out+=ipu_output_read(actual+out*16,1);ipu_service();
   if(out==total/16&&!ipu_get_state()->busy)break;
  }
  assert(out==total/16&&!ipu_get_state()->busy);
  assert(ipu_get_state()->top==0x000001b3&&(ipu_get_state()->ctrl&0x8000));
  assert(!memcmp(actual,expect,total/2)&&!memcmp(actual+total/2,expect,total/2));
  ipu_profile_t p;ipu_get_profile(&p);assert(p.csc_macroblocks==2&&p.unimplemented_commands==0);
 }
 puts("PASS IDEC two intra macroblocks, RGB32/RGB16, SGN, FIFO backpressure and checkpoint resume");
}
