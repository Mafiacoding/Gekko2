/* FRAME.FBMSK and RGB24 storage, from GSRegs.h and GSDrawScanline.cpp. */
#include <stdio.h>
#include "hw/gs_mem.c"
#include "hw/gif.c"
static unsigned failures;
#define CHECK(x,msg) do{if(!(x)){printf("FAIL: %s\n",msg);failures++;}}while(0)
static void point(unsigned prim,unsigned color){apply_ad_write(GS_REG_PRIM,prim,0);apply_ad_write(GS_REG_RGBAQ,color,0x3f800000);apply_ad_write(GS_REG_XYZ2,0x00100010,0);}
int main(void){
 gs_mem_init();gif_init();
 apply_ad_write(GS_REG_FRAME_1,0x00010000,0x0000ff00);
 gs_mem_write_psmct32(0,64,1,1,0x55667788);
 point(0,0xaa332211);
 CHECK(gs_mem_read_psmct32(0,64,1,1)==0xaa337711,"only protected green bits retain framebuffer data");
 apply_ad_write(GS_REG_FRAME_1,0x00010000,0xffffffff);
 point(0,0x12345678);
 CHECK(gs_mem_read_psmct32(0,64,1,1)==0xaa337711,"full mask preserves the entire pixel");
 apply_ad_write(GS_REG_FRAME_2,0x00010001,0xff000000);
 gs_mem_write_psmct32(2048,64,1,1,0x44123456);
 point(PRIM_CTXT_MASK,0xbbabcdef);
 CHECK(gs_mem_read_psmct32(2048,64,1,1)==0x44abcdef,"context 2 owns independent mask and buffer");
 point(0,0xdeadbeef);
 CHECK(gs_mem_read_psmct32(0,64,1,1)==0xaa337711,"context switch restores context 1 full mask");
 apply_ad_write(GS_REG_FRAME_1,0x01010000,0);
 gs_mem_write_psmct32(0,64,1,1,0x55123456);
 point(0,0xaa332211);
 CHECK(gs_mem_read_psmct32(0,64,1,1)==0x55332211,"RGB24 preserves physical alpha byte");
 apply_ad_write(GS_REG_FRAME_1,0x01010000,0x0000ff00);
 point(0,0x99665544);
 CHECK(gs_mem_read_psmct32(0,64,1,1)==0x55662244,"RGB24 combines implicit alpha protection and FBMSK");
 apply_ad_write(GS_REG_FRAME_1,0x01010000,0);
 apply_ad_write(GS_REG_ALPHA_1,0x98,0); /* (Cs - 0) * Ad / 128 + 0 */
 gs_mem_write_psmct32(0,64,1,1,0x01123456);
 point(PRIM_ABE_MASK,0x80402010);
 CHECK(gs_mem_read_psmct32(0,64,1,1)==0x01402010,"RGB24 blend destination alpha is fixed 128");
 printf("FRAME checks: %u failures\n",failures);return failures?1:0;
}
