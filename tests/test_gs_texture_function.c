#include <stdio.h>
#include <string.h>
#include "hw/gs_mem.c"
#include "hw/gif.c"
static unsigned failures;
static void check(unsigned got,unsigned want,const char *label){if(got!=want){printf("FAIL: %s got=%08x want=%08x\n",label,got,want);failures++;}}
int main(void){
 gs_mem_init();gif_init();
 /* Independent literal arithmetic: tex RGBA=(64,128,200,40),
  * vertex=(128,64,255,96). Product RGB=(64,64,398) before saturation. */
 const unsigned expected[2][4]={{0x60ff4040,0x60c88040,0x60ffa0a0,0x60ffa0a0},{0x1eff4040,0x28c88040,0x88ffa0a0,0x28ffa0a0}};
 for(unsigned tcc=0;tcc<2;tcc++)for(unsigned tfx=0;tfx<4;tfx++){
  g_gif.tex_tcc=tcc;g_gif.tex_tfx=tfx;
  check(gs_texture_function(0x28c88040,0x60ff4080),expected[tcc][tfx],"texture function");
 }
 g_gif.tex_tcc=1;g_gif.tex_tfx=2;
 check(gs_texture_function(0xf0000000,0x40000000),0xff404040,"highlight alpha saturation");
 /* Register decoding and context selection must preserve distinct TCC/TFX. */
 apply_ad_write(GS_REG_TEX0_1,1u<<14,0u);
 apply_ad_write(GS_REG_TEX0_2,1u<<14,(1u<<2)|(3u<<3));
 g_gif.prim=0;gs_activate_context();check(g_gif.tex_tcc,0,"context 1 RGB only");check(g_gif.tex_tfx,0,"context 1 modulate");
 g_gif.prim=PRIM_CTXT_MASK;gs_activate_context();check(g_gif.tex_tcc,1,"context 2 RGBA");check(g_gif.tex_tfx,3,"context 2 highlight2");
 /* TEX2 overrides only its defined fields, retaining TCC and TFX. */
 apply_ad_write(GS_REG_TEX2_2,0,0);gs_activate_context();check(g_gif.tex_tcc,1,"TEX2 retains TCC");check(g_gif.tex_tfx,3,"TEX2 retains TFX");
 puts(failures?"FAIL: texture functions":"PASS: TCC, four TFX functions, saturation and context isolation");return failures!=0;
}
