/* Common GS source semantics, no game names/assets and no GPU raster claims. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#define GEKKO2_GX_GENERIC_SHADER 1
#include "hw/gif.c"
static int index_at(int x,const gs_gx_axis_plan *p){return p->clamp?(x<0?0:(x>=(int)p->size?(int)p->size-1:x)):x&(p->size-1);}
static uint32_t atlas(double u,double v,const gs_gx_axis_plan *ax,const gs_gx_axis_plan *ay,int linear){
 if(linear){u-=.5;v-=.5;}
 int x=(int)floor(u),y=(int)floor(v);
 uint32_t c=gs_gx_atlas_texel(index_at(x,ax),index_at(y,ay));if(!linear)return c;
 unsigned fx=(unsigned)((u-x)*16),fy=(unsigned)((v-y)*16),out=0;
 uint32_t b=gs_gx_atlas_texel(index_at(x+1,ax),index_at(y,ay));
 uint32_t d=gs_gx_atlas_texel(index_at(x,ax),index_at(y+1,ay));
 uint32_t e=gs_gx_atlas_texel(index_at(x+1,ax),index_at(y+1,ay));
 for(unsigned k=0;k<32;k+=8){unsigned top=((((c>>k)&255)*(16-fx)+((b>>k)&255)*fx)>>4),bot=((((d>>k)&255)*(16-fx)+((e>>k)&255)*fx)>>4);out|=((top*(16-fy)+bot*fy)>>4)<<k;}
 return out;
}
int main(void){unsigned alphas=0,texels=0;gif_init();gs_mem_init();g_gif.colclamp=g_gif.colclamp_configured=1;
 for(unsigned tfx=0;tfx<4;tfx++)for(unsigned tcc=0;tcc<2;tcc++)for(unsigned ta=0;ta<256;ta++)for(unsigned va=0;va<256;va++){
  g_gif.tex_tfx=tfx;g_gif.tex_tcc=tcc;
  assert(gs_gx_source_alpha(ta,va,tfx,tcc,0)==(gs_texture_function((ta<<24)|0x654321,(va<<24)|0x123456)>>24));
  assert(gs_gx_source_alpha(ta,va,tfx,tcc,1)==va);alphas++;
 }
 g_gif.clamp_configured=1;g_gif.tex_tbp0=128;g_gif.tex_tbw=256;g_gif.tex_psm=0;
 for(unsigned y=0;y<256;y++)for(unsigned x=0;x<256;x++)gs_mem_write_psmct32(128*64,256,x,y,(x*1234567u)^(y*7654321u));
 const unsigned formats[]={0,1,2,10,0x13,0x14,0x1b,0x24,0x2c,0x30,0x31,0x32,0x3a};
 for(unsigned n=0;n<512;n++)g_gif.clut_cache[n]=0x80000000u+n*0x010307u;
 const double coords[]={-1025.25,-.75,0,.5,1.25,16.75,17,98.5,99.75,127.5,255.875,1024.25};
 for(unsigned fi=0;fi<13;fi++){g_gif.tex_psm=formats[fi];
 for(unsigned ex=0;ex<8;ex++)for(unsigned mx=0;mx<4;mx++)for(unsigned my=0;my<4;my++)for(unsigned linear=0;linear<2;linear++){
  g_gif.tex_tw=ex;g_gif.tex_th=7-ex;g_gif.clamp_wms=mx;g_gif.clamp_wmt=my;
  g_gif.clamp_minu=mx==2?17:21;g_gif.clamp_maxu=mx==2?99:128;
  g_gif.clamp_minv=my==2?7:10;g_gif.clamp_maxv=my==2?61:64;
  g_gif.tex1_mmag=g_gif.tex1_mmin=linear;
  gs_gx_axis_plan ax,ay;assert(gs_gx_source_axes(&ax,&ay));
  for(unsigned i=0;i<12;i++)for(unsigned j=0;j<12;j++){
   uint32_t got=atlas(coords[i],coords[j],&ax,&ay,linear),want=gs_sample_texture(coords[i],coords[j]);
   if(got!=want){printf("FAIL ex=%u mx=%u my=%u linear=%u u=%f v=%f got=%08x want=%08x sizes=%u,%u\n",ex,mx,my,linear,coords[i],coords[j],got,want,ax.size,ay.size);return 1;}texels++;
  }
 }
 }
 gs_gx_axis_plan ax,ay;g_gif.tex_psm=0;assert(!gs_gx_plan_axis(11,0,0,0,&ax));assert(!gs_gx_plan_axis(3,2,4,2,&ax));assert(!gs_gx_plan_axis(3,4,0,0,&ax));
 g_gif.tex_tw=g_gif.tex_th=2;g_gif.clamp_wms=g_gif.clamp_wmt=3;g_gif.clamp_minu=g_gif.clamp_minv=21;g_gif.clamp_maxu=g_gif.clamp_maxv=128;
 assert(gs_gx_source_axes(&ax,&ay));g_gif.fbp=0;g_gif.fbw=64;g_gif.frame_psm=0;g_gif.zbuf_configured=0;
 assert(gs_gx_atlas_disjoint(18,18,&ax,&ay));
 unsigned end=(unsigned)gs_page_end(128*256,256,ax.max_index+1,ay.max_index+1,0);
 g_gif.zbuf_configured=1;g_gif.zte=1;g_gif.zmsk=1;g_gif.zpsm=0;g_gif.zbp=(end-8192)/4;
 assert(!gs_gx_atlas_disjoint(18,18,&ax,&ay)); /* includes read-only depth aliases */
 printf("PASS %u TFX/TCC alpha pairs and %u atlas/software samples: 13 formats, tiny textures, all four clamp modes, nearest/bilinear, negative coordinates and expanded physical alias envelopes\n",alphas,texels);
}
