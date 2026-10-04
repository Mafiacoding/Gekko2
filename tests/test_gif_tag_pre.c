#include <stdio.h>
#include <string.h>
#include "hw/gs_mem.c"
#include "hw/gif.c"
static void w(unsigned char*p,unsigned v){for(unsigned i=0;i<4;i++)p[i]=v>>(8*i);}
int main(void){unsigned char b[32];unsigned fails=0;
 for(unsigned mode=0;mode<4;mode++)for(unsigned loops=0;loops<2;loops++){
  gif_init();g_gif.prim=PRIM_TYPE_TRIANGLE_STRIP;g_gif.tri_vseq=2;memset(b,0,sizeof b);
  w(b,loops|0x8000);w(b+4,(1u<<14)|(PRIM_TYPE_SPRITE<<15)|(mode<<26)|(1u<<28));w(b+8,GIF_REG_NOP);
  gif_process_quadwords(GIF_PATH_3,b,1+loops);
  unsigned expected=loops&&mode==0?PRIM_TYPE_SPRITE:PRIM_TYPE_TRIANGLE_STRIP;
  if(g_gif.prim!=expected||g_gif.tri_vseq!=(loops&&mode==0?0:2)){printf("FAIL PRE mode=%u loops=%u\n",mode,loops);fails++;}
 }
 printf("GIF PRE: %u failures\n",fails);return fails?1:0;}
