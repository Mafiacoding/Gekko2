#include "core/hw/gs_gx.h"
#include "core/hw/gs_mem.h"
#include "core/hw/gs_wii_output.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define C(x) do{if(!(x)){printf("FAIL %d\n",__LINE__);return 1;}}while(0)
static unsigned tile(unsigned w,unsigned x,unsigned y){return ((y/4)*(w/4)+x/4)*64+(y&3)*8+(x&3)*2;}
int main(void){
 unsigned bps[]={0,163840,491520};unsigned w=640,h=512;unsigned char *packed=malloc(w*h*4),*actual=malloc(GS_MEM_SIZE),*expected=malloc(GS_MEM_SIZE);C(packed&&actual&&expected);
 for(unsigned k=0;k<3;k++){
  gs_mem_init();unsigned char *vram=gs_mem_get();
  for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)gs_mem_write_psmct32(bps[k],w,x,y,0x7f000000u|((x*19u+y*13u)&255u)|(((x^y)&255u)<<8)|(((y*7u+x/64u)&255u)<<16));
  memcpy(expected,vram,GS_MEM_SIZE);memset(actual,0,GS_MEM_SIZE);
  C(gs_gx_pack_rgba8(packed,w*h*4,bps[k],w,0,0,w,h)==w*h*4);
  for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
   unsigned t=tile(w,x,y),c=gs_mem_read_psmct32(bps[k],w,x,y);
   C(packed[t]==255&&packed[t+1]==(c&255)&&packed[t+32]==((c>>8)&255)&&packed[t+33]==((c>>16)&255));
  }
  C(gs_gx_unpack_psmct32(actual,GS_MEM_SIZE,packed,w*h*4,bps[k],w,0,0,w,h,0x7f));
  C(!memcmp(actual,expected,GS_MEM_SIZE));
  /* Nonzero scanout origin and odd cropped width/height exercise padding. */
  C(gs_gx_pack_rgba8(packed,w*h*4,bps[k],w,7,11,629,497)==632*500*4);
  for(unsigned y=0;y<500;y++)for(unsigned x=0;x<632;x++){
   unsigned t=tile(632,x,y),c=gs_mem_read_psmct32(bps[k],w,7+(x<629?x:628),11+(y<497?y:496));
   C(packed[t+1]==(c&255)&&packed[t+32]==((c>>8)&255)&&packed[t+33]==((c>>16)&255));
  }
 }
 unsigned x,y,sw,sh;gs_decode_display_region(0x1400,0x1ff9ff01848290ull,3,&x,&y,&sw,&sh);C(x==0&&y==0&&sw==640&&sh==256);
 uint32_t stats[6];gs_mem_init();for(unsigned yy=0;yy<8;yy++)for(unsigned xx=0;xx<8;xx++)gs_mem_write_psmct32(0,64,xx,yy,0xff204080);
 gs_scanout_probe_rgb(0,64,0,0,8,8,stats);C(stats[0]==64&&stats[1]==128&&stats[2]==64&&stats[3]==32&&stats[5]==64);
 gs_scanout_probe_rgb(0,64,0,0,0,8,stats);C(stats[0]==0);
 free(packed);free(actual);free(expected);puts("PASS CPU framebuffer -> GX tiled -> full VRAM: three BIOS bases, every pixel/edge, odd crop padding, logged FFMD scanout");return 0;
}
