#include <stdio.h>
#include <string.h>
#include "hw/gs_mem.c"
#include "hw/gif.c"
static void w(unsigned char*p,unsigned v){for(unsigned i=0;i<4;i++)p[i]=v>>(i*8);}
static void ad(unsigned char*b,unsigned*n,unsigned lo,unsigned hi,unsigned reg){unsigned char*p=b+16+16*(*n)++;w(p,lo);w(p+4,hi);w(p+8,reg);}
int main(void){unsigned failures=0;
 for(unsigned linear=0;linear<2;linear++){
  gs_mem_init();gif_init();for(unsigned y=0;y<2;y++){gs_mem_write_psmct32_blk(2000,64,0,y,0x80000000);gs_mem_write_psmct32_blk(2000,64,1,y,0x80000080);}
  unsigned char b[176]={0};unsigned n=0;
  ad(b,&n,10u<<16,0,GS_REG_FRAME_1);ad(b,&n,2000u|(1u<<14)|(1u<<26)|(1u<<30),(1u<<2)|(TEX_TFX_DECAL<<3),GS_REG_TEX0_1);
  ad(b,&n,0,0,GS_REG_CLAMP_1); /* Enable repeat at the V=0 texture boundary. */
  ad(b,&n,linear?0x60u:0u,0,GS_REG_TEX1_1);ad(b,&n,PRIM_TYPE_SPRITE|PRIM_TME_MASK|PRIM_FST_MASK,0,GS_REG_PRIM);
  ad(b,&n,0x80808080,0,GS_REG_RGBAQ);ad(b,&n,16,0,GS_REG_UV);ad(b,&n,0,0,GS_REG_XYZ2);ad(b,&n,32u|(32u<<16),0,GS_REG_XYZ2);
  w(b,n|0x8000u);w(b+4,1u<<28);w(b+8,GIF_REG_AD);gif_process_quadwords(GIF_PATH_3,b,n+1);
  unsigned expected=linear?0x80000040u:0x80000080u;
  if(gs_mem_read_psmct32(0,640,0,0)!=expected){printf("FAIL actual UV sprite filtering mode=%u got=%08x expected=%08x\n",linear,gs_mem_read_psmct32(0,640,0,0),expected);failures++;}
  if(linear){unsigned uv=gs_sample_texture(1,.5);g_gif.prim&=~PRIM_FST_MASK;unsigned st=gs_sample_texture(1,.5);if(uv!=0x80000040u||uv!=st){puts("FAIL coordinate-mode-independent four-tap filtering");failures++;}}
 }
 printf("UV bilinear filtering: %u failures\n",failures);return failures?1:0;}
