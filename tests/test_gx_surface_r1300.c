#include "core/hw/gs_gx_surface.h"
#include "core/hw/gs_mem.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static int failures;
#define CHECK(c) do{if(!(c)){printf("FAIL line %d\n",__LINE__);failures++;}}while(0)
static uint32_t tile(uint32_t w,uint32_t x,uint32_t y){return ((y/4)*(w/4)+x/4)*64+(y&3)*8+(x&3)*2;}
int main(void){
 gs_gx_surface *s=malloc(sizeof(*s)),*saved=malloc(sizeof(*s));uint8_t *expected=malloc(GS_MEM_SIZE),*actual=malloc(GS_MEM_SIZE),pixels[128*64*4];CHECK(s&&saved&&expected&&actual);if(failures)return 1;
 gs_mem_init();uint8_t *vram=gs_mem_get();for(unsigned i=0;i<GS_MEM_SIZE;i++)vram[i]=(i*73+i/13)&255;
 memcpy(actual,vram,GS_MEM_SIZE);CHECK(gs_gx_surface_begin(s,64,128,128,64));memset(pixels,0,sizeof(pixels));
 for(unsigned y=0;y<64;y++)for(unsigned x=0;x<128;x++){uint32_t p=gs_mem_read_psmct32(64,128,x,y),t=tile(128,x,y);pixels[t]=255;pixels[t+1]=p;pixels[t+32]=p>>8;pixels[t+33]=p>>16;}
 for(unsigned n=0;n<160;n++){
  int x=(n*17)%80,y=(n*7)%32;int32_t xy[6]={x,y,x+32,y+3,x+2,y+28};gs_gx_flat_draw d;uint32_t color=0x12345678u+n*0x19283741u;
  CHECK(gs_gx_prepare_flat(&d,3,64,128,x,y,x+32,y+28,xy,color,n%4));d.psm=n%2;
  CHECK(gs_gx_surface_mark(s,&d));
  for(unsigned yy=0;yy<d.height;yy++)for(unsigned xx=d.left[yy];xx<d.right[yy];xx++){
   unsigned xx1=x+xx,yy1=y+yy,t=tile(128,xx1,yy1);uint32_t old=gs_mem_read_psmct32(64,128,xx1,yy1);
   pixels[t+1]=color;pixels[t+32]=color>>8;pixels[t+33]=color>>16;
   gs_mem_write_psmct32(64,128,xx1,yy1,d.psm?(color&0xffffff)|(old&0xff000000):color);
  }
 }
 memcpy(expected,vram,GS_MEM_SIZE);memcpy(saved,s,sizeof(*s));CHECK(!gs_gx_surface_import(s,actual,GS_MEM_SIZE,pixels,sizeof(pixels)-1));CHECK(!memcmp(saved,s,sizeof(*s)));
 gs_gx_flat_draw bad={0};bad.bp=64;bad.bw=128;bad.x=127;bad.width=4;bad.height=4;CHECK(!gs_gx_surface_mark(s,&bad));CHECK(!memcmp(saved,s,sizeof(*s)));
 CHECK(gs_gx_surface_import(s,actual,GS_MEM_SIZE,pixels,sizeof(pixels)));CHECK(!memcmp(expected,actual,GS_MEM_SIZE));CHECK(!s->active);
 CHECK(!gs_gx_surface_begin(s,0xffffffff,128,128,64));free(s);free(saved);free(expected);free(actual);
 printf("160 overlapping surface draws, mixed CT32/CT24 alpha, SCANMSK, full VRAM and atomic rejection: %d failures\n",failures);return !!failures;
}
