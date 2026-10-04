/* Natural 64-bit TEX0/CLAMP in PACKED GIF payloads, including split
 * DMA links. Primary reference: GSState.cpp ResetHandlers maps these
 * four packed registers to the same handlers as A+D. */
#include <stdio.h>
#include <string.h>
#include "hw/gs_mem.c"
#include "hw/gif.c"
static unsigned fails;
#define CHECK(c,m) do { if(!(c)){printf("FAIL: %s\n",m);fails++;} } while(0)
static void put(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static void packed(unsigned ctx){
 uint8_t p[48]={0};unsigned tex=ctx?GS_REG_TEX0_2:GS_REG_TEX0_1,clamp=ctx?GS_REG_CLAMP_2:GS_REG_CLAMP_1;
 put(p,1u|(1u<<15));put(p+4,2u<<28);put(p+8,tex|(clamp<<4));
 put(p+16,(2000u+ctx*100u)|(1u<<14)|(2u<<26)|(2u<<30));put(p+20,(1u<<2)|(TEX_TFX_DECAL<<3));
 put(p+24,0xdeadbeef);put(p+28,0xfeedface); /* upper half must be ignored */
 /* Region clamp U and V to texel (3,2). */
 uint64_t c=2u|(2u<<2)|((uint64_t)3<<4)|((uint64_t)3<<14)|((uint64_t)2<<24)|((uint64_t)2<<34);
 put(p+32,(uint32_t)c);put(p+36,(uint32_t)(c>>32));put(p+40,~0u);put(p+44,~0u);
 gif_process_quadwords(GIF_PATH_3,p,1); /* tag */
 gif_process_quadwords(GIF_PATH_3,p+16,1); /* TEX0 */
 gif_process_quadwords(GIF_PATH_3,p+32,1); /* CLAMP */
}
static void draw(unsigned ctx){
 apply_ad_write(ctx?GS_REG_FRAME_2:GS_REG_FRAME_1,1u<<16,0);
 apply_ad_write(ctx?GS_REG_XYOFFSET_2:GS_REG_XYOFFSET_1,0,0);
 apply_ad_write(ctx?GS_REG_SCISSOR_2:GS_REG_SCISSOR_1,7u<<16,7u<<16);
 apply_ad_write(GS_REG_PRIM,PRIM_TYPE_SPRITE|PRIM_TME_MASK|PRIM_FST_MASK|(ctx?PRIM_CTXT_MASK:0),0);
 apply_ad_write(GS_REG_RGBAQ,0x80808080,0x3f800000);
 apply_ad_write(GS_REG_UV,0,0);apply_ad_write(GS_REG_XYZ2,0,0);
 apply_ad_write(GS_REG_UV,0,0);apply_ad_write(GS_REG_XYZ2,(4u<<4)|((4u<<4)<<16),0);
}
int main(void){
 gs_mem_init();gif_init();
 gs_mem_write_psmct32_blk(2000,64,3,2,0x80332211);gs_mem_write_psmct32_blk(2100,64,3,2,0x80665544);
 packed(0);packed(1);
 CHECK(g_gif.ctx1_tex_tbp0==2000 && g_gif.ctx2_tex_tbp0==2100,"packed TEX0 updates both contexts separately");
 CHECK(g_gif.ctx1_clamp_minu==3 && g_gif.ctx2_clamp_minv==2,"packed CLAMP uses natural low 64 bits");
 draw(0);CHECK(gs_mem_read_psmct32(0,64,1,1)==0x80332211,"context 1 samples uploaded texel with packed region clamp");
 draw(1);CHECK(gs_mem_read_psmct32(0,64,1,1)==0x80665544,"context 2 samples its own texture and clamp");
 printf("packed TEX0/CLAMP %s\n",fails?"FAIL":"PASS");return fails?1:0;
}
