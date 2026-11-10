/* Synthetic texture/depth-independent POINT regression; no game data. */
#include <stdio.h>
#include "hw/gif.c"
int main(void){
 unsigned checks=0;gs_mem_init();gif_init();
 const unsigned psms[]={0,1,2,10};
 for(unsigned f=0;f<4;f++)for(unsigned fst=0;fst<2;fst++){
  gif_init();g_gif.ctx1_fbw=64;g_gif.ctx1_fbp=0;g_gif.ctx1_frame_psm=0;
  g_gif.ctx1_tex_tbp0=64;g_gif.ctx1_tex_tbw=64;g_gif.ctx1_tex_tw=g_gif.ctx1_tex_th=2;
  g_gif.ctx1_tex_psm=psms[f];g_gif.ctx1_tex_tfx=TEX_TFX_DECAL;g_gif.ctx1_tex_tcc=1;
  g_gif.prim=PRIM_TYPE_POINT|PRIM_TME_MASK|(fst?PRIM_FST_MASK:0);
  g_gif.cur_u=3;g_gif.cur_v=2;g_gif.cur_s=1.5f;g_gif.cur_t=1.0f;g_gif.cur_q=2.0f;
  g_gif.texa_ta0=0x40;g_gif.texa_ta1=0xa0;
  unsigned expected;
  if(f<2){gs_mem_write_psmct32(4096,64,3,2,0x803f7fc0);expected=f==0?0x803f7fc0:0x403f7fc0;}
  else {if(f==2)gs_mem_write_psmct16(4096,64,3,2,0x8abc);else gs_mem_write_psmct16s(4096,64,3,2,0x8abc);
   expected=0xa0000000u|((0x8abc&31)<<3)|(((0x8abc>>5)&31)<<11)|(((0x8abc>>10)&31)<<19);}
  gs_mem_write_psmct32(0,64,8,8,0);rasterize_point(8,8,0x80808080,0,255);
  unsigned got=gs_mem_read_psmct32(0,64,8,8);checks++;
  if(got!=expected){printf("FAIL format=%u fst=%u got=%08x expected=%08x\n",psms[f],fst,got,expected);return 1;}
 }
 printf("PASS %u textured POINT UV/STQ, CT32/24/16/16S and TEXA cases\n",checks);return 0;
}
