#include <assert.h>
#include <stdio.h>
#include "hw/gif.c"
static void vertex(unsigned x,double u,float s,float q,int draw){
 g_gif.cur_u=u;g_gif.cur_v=0;g_gif.cur_s=s;g_gif.cur_t=0;g_gif.cur_q=q;
 apply_xyz2_kick(x*16,12*16,0,draw);
}
int main(void){unsigned checks=0;gs_mem_init();
 for(unsigned strip=0;strip<2;strip++)for(unsigned fst=0;fst<2;fst++)for(unsigned variable=0;variable<2;variable++){
  gif_init();g_gif.ctx1_fbw=64;g_gif.ctx1_frame_psm=0;
  g_gif.ctx1_tex_tbp0=64;g_gif.ctx1_tex_tbw=64;g_gif.ctx1_tex_tw=2;g_gif.ctx1_tex_th=0;
  g_gif.ctx1_clamp_configured=1;g_gif.ctx1_tex_tcc=1;g_gif.ctx1_tex_tfx=TEX_TFX_DECAL;
  g_gif.prim=(strip?PRIM_TYPE_LINE_STRIP:PRIM_TYPE_LINE)|PRIM_TME_MASK|(fst?PRIM_FST_MASK:0);
  g_gif.rgba=0x80808080;
  for(unsigned x=0;x<4;x++)gs_mem_write_psmct32(4096,64,x,0,0x80010210+x);
  for(unsigned x=10;x<19;x++)gs_mem_write_psmct32(0,64,x,12,0);
  vertex(10,0,0,1,0);vertex(14,4,variable?4:1,variable?4:1,1);
  for(unsigned i=0;i<=4;i++){
   double t=i/4.0,q=1+t*(variable?3:0),u=fst?i:t*(variable?4:1)*4/q;
   unsigned expected=0x80010210+((unsigned)u&3u);
   unsigned got=gs_mem_read_psmct32(0,64,10+i,12);if(got!=expected){printf("FAIL strip=%u fst=%u variable=%u i=%u got=%08x expected=%08x u=%f\n",strip,fst,variable,i,got,expected,u);return 1;}checks++;
  }
  if(strip){vertex(18,0,0,1,1);assert(gs_mem_read_psmct32(0,64,18,12)==0x80010210);checks++;}
 }
 printf("PASS %u actual LINE/STRIP vertex-queue, XYZ3, UV/STQ and varying-Q pixel checks\n",checks);return 0;}
