#include <assert.h>
#include <stdio.h>
#define GEKKO2_GX_GENERIC_SHADER 1
#define GEKKO2_GX_EXTENDED_TRIANGLES 1
#define gs_gx_draw_native_triangle_ex capture_triangle
#define gs_gx_render_active capture_active
#define gs_gx_source_key capture_key
#include "hw/gif.c"
#undef gs_gx_draw_native_triangle_ex
#undef gs_gx_render_active
#undef gs_gx_source_key
static unsigned captured,scenario,key_lo,key_hi,key_valid;
void capture_key(uint32_t lo,uint32_t hi,int valid){key_lo=lo;key_hi=hi;key_valid=valid;}
int capture_active(void){return 1;}
int capture_triangle(uint32_t psm,uint32_t bp,uint32_t bw,int32_t minx,int32_t miny,int32_t maxx,int32_t maxy,
 const int32_t *xy,const float *uv,const float *q,const uint32_t *rgba,uint32_t tw,uint32_t th,
 uint32_t ws,uint32_t wt,uint32_t linear,uint32_t scanmsk,gs_gx_texel_fn sample,
 const gs_gx_pipeline *p,const uint32_t z[3],const gs_gx_native_color *c){
 (void)psm;(void)bp;(void)bw;(void)minx;(void)miny;(void)maxx;(void)maxy;(void)xy;(void)linear;(void)scanmsk;
 assert(c->fog_enabled&&c->fog_color==0x332211&&c->fog[0]==0&&c->fog[1]==128&&c->fog[2]==255);
 assert(rgba&&rgba[0]>>24==0&&rgba[1]>>24==128&&rgba[2]>>24==255);
 assert(p->zwrite&&p->ztest&&z[0]==1&&z[1]==20&&z[2]==40);
 if(scenario==0){assert(!c->untextured&&c->tfx==2&&tw==32&&th==8&&ws==1&&wt==0);assert(uv[2]==.5f&&uv[5]==2.f);assert(sample(31,7)==0x12345678);assert(q);}
 if(scenario==1){assert(c->untextured&&c->tfx==0&&tw==4&&th==4&&!q);assert(sample(0,0)==0x80808080);}
 if(scenario==2){assert(!c->untextured&&tw==4&&th==4&&uv[2]==4.f);assert(sample(3,3)==0x87654321);}
 captured++;return 1;
}
static void draw(void){rasterize_triangle(2,2,18,2,2,18,0x00408080,0x80408080,0xff408080,0,0,16,0,0,16,0,0,1,0,0,1,0,0,1,1,20,40,0,128,255);}
int main(void){gif_init();gs_mem_init();
 gekko2_optimization_mask|=GEKKO2_OPT_BIT(GEKKO2_OPT_GX_GOURAUD)|GEKKO2_OPT_BIT(GEKKO2_OPT_GX_RESIDENT);
 apply_ad_write(GS_REG_FRAME_1,1u<<16,0);apply_ad_write(GS_REG_ZBUF_1,0x100,0);
 apply_ad_write(GS_REG_TEST_1,(1u<<16)|(2u<<17),0);apply_ad_write(GS_REG_FOGCOL,0x332211,0);
 apply_ad_write(GS_REG_PRIM,3|PRIM_IIP_MASK|PRIM_TME_MASK|PRIM_FST_MASK|PRIM_FGE_MASK,0);
 apply_ad_write(GS_REG_TEX0_1,128u|(4u<<14)|(2u<<26)|(2u<<30),4u|(2u<<3));
 uint64_t clamp=2u|(3u<<2)|(10u<<4)|(30u<<14)|(5ull<<24)|(64ull<<34);
 apply_ad_write(GS_REG_CLAMP_1,(uint32_t)clamp,clamp>>32);gs_mem_write_psmct32(128*64,256,30,69,0x12345678);
 draw();assert(captured==1&&key_valid);
 unsigned original_lo=key_lo,original_hi=key_hi;gs_gx_native_source_key();assert(key_lo==original_lo&&key_hi==original_hi);
 gs_mem_write_psmct32(128*64,256,30,69,0x12345679);gs_gx_native_source_key();assert(key_lo!=original_lo||key_hi!=original_hi);
 gs_mem_write_psmct32(128*64,256,30,69,0x12345678);g_gif.clamp_minu=9;gs_gx_native_source_key();assert(key_lo!=original_lo||key_hi!=original_hi);
 g_gif.clamp_minu=10;g_gif.tex_psm=0x13;gs_gx_native_source_key();original_lo=key_lo;original_hi=key_hi;
 g_gif.clut_cache[5]^=1;gs_gx_native_source_key();assert(key_lo!=original_lo||key_hi!=original_hi);g_gif.tex_psm=0;

 scenario=1;apply_ad_write(GS_REG_PRIM,3|PRIM_IIP_MASK|PRIM_FGE_MASK,0);draw();assert(captured==2);
 scenario=2;apply_ad_write(GS_REG_PRIM,3|PRIM_IIP_MASK|PRIM_TME_MASK|PRIM_FST_MASK|PRIM_FGE_MASK,0);
 apply_ad_write(GS_REG_CLAMP_1,0,0);apply_ad_write(GS_REG_TEX0_1,128u|(4u<<14),4u);
 gs_mem_write_psmct32(128*64,256,0,0,0x87654321);draw();assert(captured==3);
 /* Current native texture backend has one filter bit, so mixed min/mag
  * and mip modes must use the precise software/source-shaded fallback. */
 for(unsigned mag=0;mag<2;mag++)for(unsigned min=0;min<6;min++){
  unsigned before=captured;apply_ad_write(GS_REG_TEX1_1,(mag<<5)|(min<<6),0);draw();
  assert(captured==before+(min<=1&&mag==min));
 }
 unsigned before=captured;apply_ad_write(GS_REG_TEX1_1,0,0);
 gekko2_optimization_mask&=~GEKKO2_OPT_BIT(GEKKO2_OPT_GX_RESIDENT);draw();assert(captured==before);
 puts("PASS common GIF admission: HIGHLIGHT, fog/color/vertices, varying alpha, region atlas coordinates/source, tiny textures, untextured Z and runtime gate");
}
