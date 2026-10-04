#include <stdio.h>
#include "hw/gs_mem.c"
#include "hw/gif.c"
static void begin(unsigned psm,unsigned width){apply_ad_write(GS_REG_BITBLTBUF,0,(psm<<24)|(1u<<16)|64);apply_ad_write(GS_REG_TRXREG,width,1);apply_ad_write(GS_REG_TRXDIR,0,0);}
int main(void){
 gs_mem_init();gif_init();begin(0,2);apply_ad_write(GS_REG_HWREG,0x80123456,0x80abcdef);
 if(gs_mem_read_psmct32(4096,64,0,0)!=0x80123456 || gs_mem_read_psmct32(4096,64,1,0)!=0x80abcdef)return puts("FAIL: HWREG two 32-bit texels"),1;
 begin(1,4);apply_ad_write(GS_REG_HWREG,0x04030201,0x08070605);apply_ad_write(GS_REG_HWREG,0x0c0b0a09,0xffffffff);
 if((gs_mem_read_psmct32(4096,64,0,0)&0xffffff)!=0x030201 || (gs_mem_read_psmct32(4096,64,2,0)&0xffffff)!=0x090807 || (gs_mem_read_psmct32(4096,64,3,0)&0xffffff)!=0x0c0b0a)return puts("FAIL: HWREG 24-bit partial texel carry"),1;
 begin(0x14,16);apply_ad_write(GS_REG_HWREG,0x76543210,0xfedcba98);for(unsigned i=0;i<16;i++)if(gs_mem_read_index(4096,64,i,0,0x14)!=i)return puts("FAIL: HWREG 4-bit nibble upload"),1;
 apply_ad_write(GS_REG_TRXDIR,2,0);apply_ad_write(GS_REG_HWREG,0,0);if(gs_mem_read_index(4096,64,1,0,0x14)!=1)return puts("FAIL: non host-to-local HWREG modifies VRAM"),1;
 puts("PASS: HWREG host-to-local formats, partial 24-bit carry and direction gate");return 0;
}
