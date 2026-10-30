/* Exact CPU source-shading parity, not a physical GX raster/TEV test. */
#include <assert.h>
#include <stdio.h>
#include "hw/gif.c"
int main(void){
 unsigned checked=0;
 for(unsigned mode=0;mode<64;mode++){
  gif_init();gs_mem_init();
  int32_t xy[6]={2,3,18,4,5,20};
  double uv[6]={1,2,7,0,3,7};
  float stq[9]={.1f,.2f,1.f,.8f,.05f,1.25f,.4f,.9f,.75f};
  uint32_t colors[3]={0x80123456,0x80785634,0x809abcde};
  g_gif.prim=3|PRIM_TME_MASK|((mode&1)?PRIM_FST_MASK:0)|((mode&2)?PRIM_IIP_MASK:0);
  g_gif.prmodecont_ac=1;g_gif.fbp=0;g_gif.fbw=64;g_gif.frame_psm=0;
  g_gif.tex_tbp0=16384;g_gif.tex_tbw=64;g_gif.tex_psm=0;g_gif.tex_tw=g_gif.tex_th=3;
  g_gif.tex_tfx=(mode>>2)&3;g_gif.tex_tcc=(mode>>4)&1;
  g_gif.tex1_mmag=g_gif.tex1_mmin=(mode>>5)&1;
  g_gif.colclamp=1;g_gif.colclamp_configured=1;
  g_gif.ctx1_fbp=g_gif.fbp;
  g_gif.ctx1_fbw=g_gif.fbw;
  g_gif.ctx1_frame_psm=g_gif.frame_psm;
  g_gif.ctx1_tex_tbp0=g_gif.tex_tbp0;
  g_gif.ctx1_tex_tbw=g_gif.tex_tbw;
  g_gif.ctx1_tex_psm=g_gif.tex_psm;
  g_gif.ctx1_tex_tw=g_gif.tex_tw;
  g_gif.ctx1_tex_th=g_gif.tex_th;
  g_gif.ctx1_tex_tfx=g_gif.tex_tfx;
  g_gif.ctx1_tex_tcc=g_gif.tex_tcc;
  g_gif.ctx1_tex1_mmag=g_gif.tex1_mmag;
  g_gif.ctx1_tex1_mmin=g_gif.tex1_mmin;
  for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++)gs_mem_write_psmct32(16384,64,x,y,(x*27+y*13)|((x*13+y*17)<<8)|((x*11+y*21)<<16)|((32+x*17+y*5)<<24));
  memcpy(gx_triangle.xy,xy,sizeof xy);memcpy(gx_triangle.uv,uv,sizeof uv);
  memcpy(gx_triangle.stq,stq,sizeof stq);memcpy(gx_triangle.colors,colors,sizeof colors);
  gx_triangle.inv=1.0/(double)edge(xy[0],xy[1],xy[2],xy[3],xy[4],xy[5]);gx_triangle.maxx=18;gx_triangle.maxy=20;
  rasterize_triangle(xy[0],xy[1],xy[2],xy[3],xy[4],xy[5],colors[0],colors[1],colors[2],uv[0],uv[1],uv[2],uv[3],uv[4],uv[5],stq[0],stq[1],stq[2],stq[3],stq[4],stq[5],stq[6],stq[7],stq[8],0,0,0,0,0,0);
  for(int y=3;y<=20;y++)for(int x=2;x<=18;x++){
   uint32_t source=gs_gx_triangle_sample(x,y),actual=gs_mem_read_psmct32(0,64,x,y);
   assert(source==actual);checked++;
  }
  assert(gs_gx_triangle_sample(19,20)==0&&gs_gx_triangle_sample(18,21)==0);
 }
 printf("PASS %u exact shaded-source pixels: STQ/UV, flat/Gouraud, four TFX modes, TCC, nearest/bilinear and triangle/padding coverage\n",checked);
 return 0;
}
