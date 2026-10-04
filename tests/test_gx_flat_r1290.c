#include "core/hw/gs_mem.h"
#include "core/hw/gs_gx.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t seed=1290;
static uint32_t random_word(void){seed=seed*1664525u+1013904223u;return seed;}
static int64_t edge(int ax,int ay,int bx,int by,int x,int y){return (int64_t)(bx-ax)*(y-ay)-(int64_t)(by-ay)*(x-ax);}
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint8_t expected[GS_MEM_SIZE],tiles[640*512*4];
int main(void)
{
 gs_mem_init();unsigned cases=0;
 for(unsigned k=0;k<250;k++) {
  uint32_t kind=k<200?3:6,mask=k%4,rgba=0x80112233;
  int32_t xy[6];for(unsigned i=0;i<6;i++)xy[i]=(int32_t)(random_word()%64);
  int minx=xy[0],maxx=xy[0],miny=xy[1],maxy=xy[1];
  for(unsigned i=1;i<(kind==3?3u:2u);i++){int x=xy[i*2],y=xy[i*2+1];if(x<minx)minx=x;if(x>maxx)maxx=x;if(y<miny)miny=y;if(y>maxy)maxy=y;}
  int64_t area=edge(xy[0],xy[1],xy[2],xy[3],xy[4],xy[5]);
  if((kind==3&&!area)||minx==maxx||miny==maxy)continue;
  /* Additional scissor clipping with non-aligned bounds. */
  if(minx<8)minx=8;if(miny<7)miny=7;if(maxx>55)maxx=55;if(maxy>54)maxy=54;
  if(maxx<=minx||maxy<=miny)continue;
  gs_gx_flat_draw d;CHECK(gs_gx_prepare_flat(&d,kind,64,128,minx,miny,maxx,maxy,xy,rgba,mask));
  uint8_t *vram=gs_mem_get();memset(vram,0xa5,GS_MEM_SIZE);
  for(unsigned y=0;y<d.height;y++)for(unsigned x=0;x<d.width;x++) {
   int px=minx+(int)x,py=miny+(int)y;
   int covered=px<=(kind==3?maxx:maxx-1)&&py<=(kind==3?maxy:maxy-1);
   if(kind==3){int64_t a=edge(xy[0],xy[1],xy[2],xy[3],px,py),b=edge(xy[2],xy[3],xy[4],xy[5],px,py),c=edge(xy[4],xy[5],xy[0],xy[1],px,py);covered=covered&&(area>0?(a>=0&&b>=0&&c>=0):(a<=0&&b<=0&&c<=0));}
   if((mask==2&&!(py&1))||(mask==3&&(py&1)))covered=0;
   CHECK(covered==(x>=d.left[y]&&x<d.right[y]));
   if(covered)gs_mem_write_psmct32(64,128,px,py,rgba);
   unsigned t=((y/4)*(d.width/4)+x/4)*64+(y%4)*8+(x%4)*2;
   tiles[t]=255;tiles[t+1]=0x33;tiles[t+32]=0x22;tiles[t+33]=0x11;
  }
  memcpy(expected,vram,GS_MEM_SIZE);memset(vram,0xa5,GS_MEM_SIZE);
  CHECK(gs_gx_import_flat(vram,GS_MEM_SIZE,tiles,d.width*d.height*4,&d));
  CHECK(!memcmp(vram,expected,GS_MEM_SIZE));cases++;
 }
 gs_gx_flat_draw d;int32_t xy[6]={0,0,8,0,0,8};
 CHECK(!gs_gx_prepare_flat(&d,3,0xffffffff,128,0,0,8,8,xy,0,0));
 CHECK(!gs_gx_prepare_flat(&d,1,0,128,0,0,8,8,xy,0,0));
 CHECK(gs_gx_prepare_flat(&d,3,0,128,0,0,8,8,xy,0,0));
 uint8_t *vram=gs_mem_get();memcpy(expected,vram,GS_MEM_SIZE);d.right[0]=641;
 CHECK(!gs_gx_import_flat(vram,GS_MEM_SIZE,tiles,sizeof(tiles),&d));CHECK(!memcmp(expected,vram,GS_MEM_SIZE));
 printf("PASS %u independent flat triangle/sprite coverage and full VRAM imports, clipping/winding/SCANMSK/padding/bounds\n",cases);return 0;
}
