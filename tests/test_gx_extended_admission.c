/* Verify GIF routes GS state to the experimental backend, without GPU claims. */
#include <assert.h>
#include <stdio.h>
#define GEKKO2_GX_EXTENDED_TRIANGLES 1
#define gs_gx_draw_native_triangle_ex capture_triangle
#define gs_gx_render_active capture_active
#undef GEKKO2_GX_GENERIC_SHADER /* Keep this BETA12.25 control admission contract. */
#include "hw/gif.c"
#undef gs_gx_draw_native_triangle_ex
#undef gs_gx_render_active
static unsigned captured,expected_tfx,expected_tcc=1;
int capture_active(void){return 1;}
int capture_triangle(uint32_t psm,uint32_t bp,uint32_t bw,int32_t minx,int32_t miny,int32_t maxx,int32_t maxy,
 const int32_t *xy,const float *uv,const float *q,const uint32_t *rgba,uint32_t tw,uint32_t th,
 uint32_t ws,uint32_t wt,uint32_t linear,uint32_t scanmsk,gs_gx_texel_fn sample,
 const gs_gx_pipeline *p,const uint32_t z[3],const gs_gx_native_color *color){
 (void)bp;(void)bw;(void)minx;(void)miny;(void)maxx;(void)maxy;(void)xy;(void)uv;(void)q;(void)rgba;(void)ws;(void)wt;(void)scanmsk;(void)sample;
 assert(psm==1&&tw==128&&th==1024&&linear==1);
 assert(p->ate&&p->atst==5&&p->aref==64&&p->afail==3&&p->date&&p->datm==1);
 assert(p->zwrite&&p->ztest&&p->ztst==2&&z[0]==1&&z[1]==20&&z[2]==40);
 assert(color->tfx==expected_tfx&&color->tcc==expected_tcc);
 assert((rgba!=NULL)==(expected_tfx==0));captured++;return 1;
}
static void draw(void){rasterize_triangle(2,2,18,2,2,18,0x80808080,0x80808081,0x80808082,0,0,16,0,0,16,0,0,1,0,0,1,0,0,1,1,20,40,0,0,0);}
int main(void){gif_init();gs_mem_init();
 gekko2_optimization_mask|=GEKKO2_OPT_BIT(GEKKO2_OPT_GX_GOURAUD)|GEKKO2_OPT_BIT(GEKKO2_OPT_GX_RESIDENT);
 apply_ad_write(GS_REG_FRAME_1,(1u<<16)|(1u<<24),0); /* CT24, 64 wide */
 apply_ad_write(GS_REG_TEX0_1,128u|(2u<<14)|(0x13u<<20)|(7u<<26)|(2u<<30),2u|(1u<<2));
 apply_ad_write(GS_REG_CLAMP_1,0,0);apply_ad_write(GS_REG_TEX1_1,(1u<<5)|(1u<<6),0);
 apply_ad_write(GS_REG_ZBUF_1,0x100,0);
 apply_ad_write(GS_REG_TEST_1,1u|(5u<<1)|(64u<<4)|(3u<<12)|(1u<<14)|(1u<<15)|(1u<<16)|(2u<<17),0);
 apply_ad_write(GS_REG_PRIM,PRIM_TYPE_TRIANGLE|PRIM_TME_MASK|PRIM_FST_MASK|PRIM_IIP_MASK,0);
 draw();assert(captured==1);
 expected_tfx=1;
 apply_ad_write(GS_REG_TEX0_1,128u|(2u<<14)|(0x13u<<20)|(7u<<26)|(2u<<30),2u|(1u<<2)|(1u<<3));draw();assert(captured==2);
 expected_tfx=0;expected_tcc=0;
 apply_ad_write(GS_REG_TEX0_1,128u|(2u<<14)|(0x13u<<20)|(7u<<26)|(2u<<30),2u);draw();assert(captured==3);
 /* Fog declines with otherwise disjoint eligible source. */
 apply_ad_write(GS_REG_PRIM,PRIM_TYPE_TRIANGLE|PRIM_TME_MASK|PRIM_FST_MASK|PRIM_FGE_MASK,0);draw();assert(captured==3);
 apply_ad_write(GS_REG_PRIM,PRIM_TYPE_TRIANGLE|PRIM_TME_MASK|PRIM_FST_MASK|PRIM_IIP_MASK,0);
 /* Feedback and fog retain software rendering, never call the new backend. */
 apply_ad_write(GS_REG_TEX0_1,(2u<<14)|(0x13u<<20)|(7u<<26)|(2u<<30),2u|(1u<<2));draw();assert(captured==3);
 puts("PASS GIF admission: small triangle, 128x1024, linear, variable Z, late alpha/DATE state; framebuffer feedback/fog decline");
}
