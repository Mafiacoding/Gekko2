/* Synthetic single-intra-macroblock MPEG2 reference driver. No game data. */
#include "core/hw/ipu.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
int main(int argc,char **argv){if(argc!=3)return 2;FILE *f=fopen(argv[1],"rb");if(!f)return 3;uint8_t in[1024]={0};unsigned size=fread(in,1,1000,f);fclose(f);unsigned off=0;for(unsigned i=0;i+4<size;i++)if(in[i]==0&&in[i+1]==0&&in[i+2]==1&&in[i+3]==1)off=i+5;if(!off||!(in[off-1]&1))return 1;unsigned qsc=in[off-1]>>3;
static const uint8_t q[64]={8,16,19,22,26,27,29,34,16,16,22,24,27,29,34,37,19,22,26,27,29,34,34,38,22,22,26,27,29,34,37,40,22,26,27,29,32,35,40,48,26,27,29,32,35,40,48,58,26,27,29,34,38,46,56,69,27,29,35,38,46,56,69,83};
static const uint8_t scan[64]={0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63};
ipu_init();ipu_state_t *s=ipu_get_state();for(unsigned i=0;i<64;i++)s->iq[0][i]=q[scan[i]];
for(unsigned i=0;i<size-off;i++)in[i]=in[i+off];size-=off;
in[size++]=0;in[size++]=0;in[size++]=1;in[size++]=0xb7;while(size&15)in[size++]=0;
ipu_mmio_write32(0x10002000,(0x2c000000|(qsc<<16)));unsigned sent=0,got=0;uint8_t out[768]={0};
for(unsigned g=0;g<100;g++){if(sent<size/16)sent+=ipu_input_write(in+sent*16,1);ipu_service();got+=ipu_output_read(out+got*16,48-got);ipu_service();if(got==48&&!s->busy)break;}
f=fopen(argv[2],"wb");for(unsigned i=0;i<384;i++)fputc(out[i*2],f);fclose(f);printf("sent=%u got=%u busy=%u ctrl=%x b=%u idx=%u top=%x\n",sent,got,s->busy,s->ctrl,s->mpeg.block,s->mpeg.index,s->top);return got!=48||s->busy;}
