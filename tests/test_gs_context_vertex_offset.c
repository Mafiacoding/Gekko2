/* Alternating drawing contexts with different XYOFFSET values: both
 * vertices must be assembled with the selected context's offset. */
#include <stdio.h>
#include "hw/gs_mem.c"
#include "hw/gif.c"
static unsigned fails;
#define CHECK(c,m) do {if(!(c)){printf("FAIL: %s\n",m);fails++;}}while(0)
static void draw(unsigned ctx,unsigned x,unsigned y){
 unsigned ox=ctx?128:32,oy=ctx?112:16;
 apply_ad_write(GS_REG_PRIM,PRIM_TYPE_SPRITE|(ctx?PRIM_CTXT_MASK:0),0);
 apply_ad_write(GS_REG_RGBAQ,ctx?0x8000ff00:0x800000ff,0);
 apply_ad_write(GS_REG_XYZ2,((x+ox)<<4)|(((y+oy)<<4)<<16),0);
 apply_ad_write(GS_REG_XYZ2,((x+ox+4)<<4)|(((y+oy+4)<<4)<<16),0);
}
int main(void){
 gs_mem_init();gif_init();
 apply_ad_write(GS_REG_FRAME_1,1u<<16,0);apply_ad_write(GS_REG_FRAME_2,1u<<16,0);
 apply_ad_write(GS_REG_XYOFFSET_1,32u<<4,16u<<4);
 apply_ad_write(GS_REG_XYOFFSET_2,128u<<4,112u<<4);
 apply_ad_write(GS_REG_SCISSOR_1,31u<<16,31u<<16);apply_ad_write(GS_REG_SCISSOR_2,31u<<16,31u<<16);
 draw(0,2,2);CHECK(gs_mem_read_psmct32(0,64,3,3)==0x800000ff,"first context ignores last written inactive XYOFFSET");
 draw(1,10,10);CHECK(gs_mem_read_psmct32(0,64,11,11)==0x8000ff00,"switch to context 2 uses its offset for first vertex");
 draw(0,20,20);CHECK(gs_mem_read_psmct32(0,64,21,21)==0x800000ff,"switch back uses context 1 offset for first vertex");
 CHECK(g_gif.sprites_drawn==3,"all three alternating sprites drawn");
 printf("vertex context offset %s\n",fails?"FAIL":"PASS");return fails?1:0;
}
