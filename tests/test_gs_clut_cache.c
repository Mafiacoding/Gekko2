#include <stdio.h>
#include "hw/gs_mem.c"
#include "hw/gif.c"
static int failed;
#define CHECK(c,msg) do{if(!(c)){puts("FAIL: " msg);failed++;}}while(0)
static void tex(unsigned reg,unsigned psm,unsigned cbp,unsigned cpsm,unsigned csm,unsigned csa,unsigned cld){apply_ad_write(reg,psm<<20,(cbp<<5)|(cpsm<<19)|(csm<<23)|(csa<<24)|(cld<<29));}
int main(void){
 gs_mem_init();gif_init();unsigned cbp=100;
 /* Literal 8x2 source coordinates, distinct second row. CSA selects cache. */
 for(unsigned y=0;y<2;y++)for(unsigned x=0;x<8;x++)gs_mem_write_psmct32(cbp*64,64,x,y,0x80010000+y*8+x);
 tex(GS_REG_TEX0_1,0x14,cbp,0,0,3,1);
 for(unsigned i=0;i<16;i++)CHECK(gs_sample_clut(i)==0x80010000+i,"CSM1 I4 8x2 source and CSA cache destination");
 gs_mem_write_psmct32(cbp*64,64,0,1,0x80abcdef);tex(GS_REG_TEX2_1,0x14,cbp,0,0,3,0);CHECK(gs_sample_clut(8)==0x80010008,"CLD0 keeps cached palette after VRAM changes");
 tex(GS_REG_TEX2_1,0x14,cbp,0,0,3,2);CHECK(gs_sample_clut(8)==0x80abcdef,"CLD2 loads and updates CBP0");
 gs_mem_write_psmct32(cbp*64,64,0,1,0x80123456);tex(GS_REG_TEX2_1,0x14,cbp,0,0,3,4);CHECK(gs_sample_clut(8)==0x80abcdef,"CLD4 same CBP0 does not reload");
 tex(GS_REG_TEX2_1,0x14,cbp,0,0,3,1);CHECK(gs_sample_clut(8)==0x80123456,"CLD1 always reloads");
 tex(GS_REG_TEX2_2,0x14,cbp,0,0,3,3);CHECK(g_gif.clut_cbp1==cbp,"context2 CLD3 updates shared CBP1");
 /* All 256 I8 entries use the documented 16x16 arrangement. */
 for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++)gs_mem_write_psmct32(cbp*64,64,x,y,0x80020000+y*16+x);
 tex(GS_REG_TEX0_1,0x13,cbp,0,0,0,1);
 for(unsigned i=0;i<256;i++){unsigned x=(i&7)|((i&16)>>1),y=((i&8)>>3)|((i&224)>>4);CHECK(gs_sample_clut(i)==0x80020000+y*16+x,"CSM1 I8 documented coordinates");}
 /* CSM2 uses TEXCLUT row/column, without an index-bit permutation. */
 apply_ad_write(GS_REG_TEXCLUT,2|(3<<6)|(5<<12),0);g_gif.texa_ta1=0x80;
 for(unsigned i=0;i<16;i++)gs_mem_write_psmct16(cbp*64,128,48+i,5,0x8000|i);
 tex(GS_REG_TEX0_1,0x14,cbp,2,1,0,1);
 CHECK(gs_sample_clut(0)==0x80000000,"CSM2 TEXCLUT origin");CHECK(gs_sample_clut(15)==0x80000078,"CSM2 linear row and TEXA expansion");
 puts(failed?"FAIL: CLUT cache":"PASS: CLUT source layout, CSA, CLD cache and CSM2");return failed!=0;
}
