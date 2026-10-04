/* Fixed-point four-tap STQ filtering with independent wrap on each tap. */
#include <stdio.h>
#include "hw/gs_mem.c"
#include "hw/gif.c"
static unsigned failures;
#define CHECK(x,m) do { if(!(x)){printf("FAIL: %s\n",m);failures++;}}while(0)
int main(void){
 gs_mem_init();gif_init();
 g_gif.tex_tbp0=3000;g_gif.tex_tbw=64;g_gif.tex_tw=1;g_gif.tex_th=1;g_gif.tex_psm=0;
 g_gif.tex1_mmag=1;g_gif.tex1_mmin=1;
 g_gif.clamp_configured=1;g_gif.clamp_wms=GS_CLAMP_CLAMP;g_gif.clamp_wmt=GS_CLAMP_CLAMP;
 uint32_t c[4]={0x20000000,0x60402080,0xa08040c0,0xe0c06000};
 for(unsigned y=0;y<2;y++)for(unsigned x=0;x<2;x++)gs_mem_write_psmct32_blk(3000,64,x,y,c[y*2+x]);
 CHECK(gs_sample_texture(0.5,0.5)==c[0],"texel center samples exact first texel");
 CHECK(gs_sample_texture(1.5,1.5)==c[3],"texel center samples exact last texel");
 CHECK(gs_sample_texture(1.0,1.0)==0x80603050,"halfway between four RGBA texels averages all channels");
 CHECK(gs_sample_texture(0.0,0.5)==c[0],"clamp each negative-boundary tap to texel zero");
 g_gif.clamp_wms=GS_CLAMP_REPEAT;
 CHECK(gs_sample_texture(0.0,0.5)==0x40201040,"repeat blends last and first texel across negative boundary");
 g_gif.clamp_wms=GS_CLAMP_REGION_CLAMP;g_gif.clamp_minu=1;g_gif.clamp_maxu=1;
 CHECK(gs_sample_texture(0.5,0.5)==c[1],"region clamp applies to both taps");
 g_gif.clamp_wms=GS_CLAMP_CLAMP;g_gif.tex1_mmag=0;g_gif.tex1_mmin=0;
 CHECK(gs_sample_texture(0.0,0.0)==c[0],"nearest mode preserved");
 g_gif.tex1_mmag=1;g_gif.tex1_mmin=1;
 CHECK(gs_sample_texture(0.75,0.5)==0x30100820,"quarter texel uses 4-bit fractional weights");
 for(unsigned mode=0;mode<2;mode++){
  g_gif.tex_psm=mode?TEX_PSM_PSMT4:TEX_PSM_PSMT8;g_gif.tex_tbw=128;g_gif.tex_cpsm=0;g_gif.tex_csa=0;g_gif.tex_csm=0;
  for(unsigned y=0;y<2;y++)for(unsigned x=0;x<2;x++){
   unsigned index=y*2+x;g_gif.clut_cache[index]=c[index];
   gs_mem_write_index(3000*64u,128,x,y,g_gif.tex_psm,index);
  }
  CHECK(gs_sample_texture(1.0,1.0)==0x80603050,"indexed filtering averages resolved palette colors");
 }
 printf("STQ linear filtering: %u failures\n",failures);return failures?1:0;
}
