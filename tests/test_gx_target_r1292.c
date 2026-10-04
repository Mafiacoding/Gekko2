#include "core/hw/gs_gx.h"
#include "core/hw/gs_mem.h"
#include <stdio.h>
static unsigned seed=1292;
static unsigned rnd(void){seed=seed*1664525u+1013904223u;return seed;}
/* Independent table representation of the GS page/block/column layout. */
static unsigned long long off(unsigned bp,unsigned bw,unsigned x,unsigned y){
 static const unsigned bx[8]={0,1,4,5,16,17,20,21},by[4]={0,2,8,10};
 static const unsigned wx[8]={0,1,4,5,8,9,12,13},wy[8]={0,2,16,18,32,34,48,50};
 return (unsigned long long)bp*4+((unsigned long long)(y/32)*(bw/64)+x/64)*8192+
 (bx[(x%64)/8]+by[(y%32)/8])*256+(wx[x%8]+wy[y%8])*4;
}
static int oracle(unsigned size,unsigned bp,unsigned bw,unsigned x,unsigned y,unsigned w,unsigned h){
 if(size!=GS_MEM_SIZE||!w||!h||w>640||h>512||(w&3)||(h&3)||x>2048-w||y>2048-h||!bw||bw>2048||(bw&63))return 0;
 for(unsigned j=0;j<h;j++)for(unsigned i=0;i<w;i++)if(off(bp,bw,x+i,y+j)>size-4)return 0;
 return 1;
}
int main(void){
 for(unsigned n=0;n<6000;n++){
 unsigned w=4*(1+rnd()%32),h=4*(1+rnd()%32),x=rnd()%2048,y=rnd()%2048,bw=64*(1+rnd()%32);
 unsigned bp=(n%3==0)?rnd():rnd()%(GS_MEM_SIZE/4);
 if(n%3==2){unsigned long long end=off(0,bw,x+w-1,y+h-1);bp=end<GS_MEM_SIZE?(GS_MEM_SIZE-(unsigned)end)/4:0;}
 if(n%11==0)w++;if(n%13==0)bw++;if(n%17==0)x=0xffffffff;
 int want=oracle(GS_MEM_SIZE,bp,bw,x,y,w,h);
 if(gs_gx_target_valid(GS_MEM_SIZE,bp,bw,x,y,w,h)!=want){printf("FAIL case %u\n",n);return 1;}
 }
 for(unsigned bw=64;bw<=2048;bw+=64)for(unsigned x=0;x<2048;x++)for(unsigned y=0;y<2048;y++){
 unsigned long long a=off(0,bw,x,y);
 if((x<2047&&a>=off(0,bw,x+1,y))||(y<2047&&a>=off(0,bw,x,y+1))){puts("FAIL monotonic page boundary");return 1;}
 }
 puts("PASS 6000 exhaustive-pixel rectangle oracle cases and all 2048x2048 coordinate transitions for 32 framebuffer widths");return 0;
}
