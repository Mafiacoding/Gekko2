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
 /* Partial tiled imports must equal the full import, including untouched
  * VRAM and the independent exact CT32/CT24 alpha shadow. */
 {
  memcpy(s,saved,sizeof(*s));memcpy(actual,vram,GS_MEM_SIZE);
  uint32_t x=s->dirty_x0&~3u,y=s->dirty_y0&~3u;
  uint32_t w=((s->dirty_x1+3u)&~3u)-x,h=((s->dirty_y1+3u)&~3u)-y;
  uint8_t *part=malloc(w*h*4u);CHECK(part);if(!part)return 1;
  for(unsigned yy=0;yy<h;yy++)for(unsigned xx=0;xx<w;xx++) {
   unsigned a=tile(128,x+xx,y+yy),b=tile(w,xx,yy);
   part[b]=pixels[a];part[b+1]=pixels[a+1];part[b+32]=pixels[a+32];part[b+33]=pixels[a+33];
  }
  CHECK(!gs_gx_surface_import_rect(s,actual,GS_MEM_SIZE,part,w*h*4u-1,x,y,w,h));
  CHECK(s->active);CHECK(!gs_gx_surface_import_rect(s,actual,GS_MEM_SIZE,part,w*h*4u,x+4,y,w-4,h));
  CHECK(gs_gx_surface_import_rect(s,actual,GS_MEM_SIZE,part,w*h*4u,x,y,w,h));
  CHECK(!memcmp(expected,actual,GS_MEM_SIZE));free(part);
 }
 for(unsigned n=0;n<100;n++) {
  memcpy(actual,vram,GS_MEM_SIZE);CHECK(gs_gx_surface_begin(s,64,128,128,64));
  gs_gx_flat_draw d={0};d.bp=64;d.bw=128;d.x=(n*11)%116;d.y=(n*7)%52;
  d.width=9;d.height=9;d.psm=n&1;d.rgba=0x81336699;
  for(unsigned yy=0;yy<9;yy++){d.left[yy]=yy%3;d.right[yy]=9-yy%2;}
  CHECK(gs_gx_surface_mark(s,&d));
  uint32_t x=s->dirty_x0&~3u,y=s->dirty_y0&~3u;
  uint32_t w=((s->dirty_x1+3u)&~3u)-x,h=((s->dirty_y1+3u)&~3u)-y;
  uint8_t part[16*16*4];memset(part,0,sizeof(part));
  for(unsigned yy=0;yy<h;yy++)for(unsigned xx=0;xx<w;xx++){
   unsigned t=tile(w,xx,yy);part[t]=255;part[t+1]=0x99;part[t+32]=0x66;part[t+33]=0x33;
  }
  for(unsigned yy=0;yy<9;yy++)for(unsigned xx=d.left[yy];xx<d.right[yy];xx++) {
   uint32_t old=gs_mem_read_psmct32(64,128,d.x+xx,d.y+yy);
   gs_mem_write_psmct32(64,128,d.x+xx,d.y+yy,d.psm?(old&0xff000000)|0x336699:d.rgba);
  }
  CHECK(gs_gx_surface_import_rect(s,actual,GS_MEM_SIZE,part,sizeof(part),x,y,w,h));
  CHECK(!memcmp(vram,actual,GS_MEM_SIZE));
 }
 CHECK(!gs_gx_surface_begin(s,0xffffffff,128,128,64));free(s);free(saved);free(expected);free(actual);
 printf("160 overlapping surface draws, mixed CT32/CT24 alpha, SCANMSK, full VRAM and atomic rejection: %d failures\n",failures);return !!failures;
}
