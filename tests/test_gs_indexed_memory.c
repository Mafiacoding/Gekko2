#include <stdio.h>
#include "hw/gif.c"
int main(void){
 gs_mem_init();
 gs_mem_write_index(0,128,1,0,0x13,0xab);gs_mem_write_index(0,128,0,2,0x13,0xcd);
 if(gs_mem_get()[4]!=0xab||gs_mem_get()[33]!=0xcd)return puts("FAIL literal PSMT8 columns"),1;
 gs_mem_write_index(0,128,1,0,0x14,0x5);gs_mem_write_index(0,128,0,2,0x14,0xe);
 if((gs_mem_get()[4]&15)!=5||(gs_mem_get()[32]>>4)!=14)return puts("FAIL literal PSMT4 nibble columns"),1;
 gs_mem_write_psmct32(0,64,0,0,0x12345678);
 gs_mem_write_index(0,64,0,0,0x24,0xa);gs_mem_write_index(0,64,0,0,0x2c,0xb);
 if(gs_mem_read_psmct32(0,64,0,0)!=0xba345678)return puts("FAIL high-nibble indexed views preserve RGB"),1;
 for(unsigned p=0;p<2;p++){
  unsigned fmt=p?0x14:0x13,mask=p?15:255,height=p?128:64;
  gs_mem_init();
  for(unsigned y=0;y<height;y++)for(unsigned x=0;x<128;x++)gs_mem_write_index(0,128,x,y,fmt,(x+3*y)&mask);
  for(unsigned y=0;y<height;y++)for(unsigned x=0;x<128;x++)if(gs_mem_read_index(0,128,x,y,fmt)!=((x+3*y)&mask))return puts("FAIL indexed page address collision"),1;
 }
 gif_init();gs_mem_init();apply_ad_write(GS_REG_BITBLTBUF,0,(2u<<16)|(0x14u<<24));apply_ad_write(GS_REG_TRXREG,3,1);apply_ad_write(GS_REG_TRXDIR,0,0);
 unsigned char pixels[16]={0x21,0x43};image_write_pixel_qwords(pixels,1);
 if(g_gif.trx_active||gs_mem_read_index(0,128,0,0,0x14)!=1||gs_mem_read_index(0,128,1,0,0x14)!=2||gs_mem_read_index(0,128,2,0,0x14)!=3||gs_mem_read_index(0,128,3,0,0x14)!=0)return puts("FAIL packed PSMT4 upload and odd rectangle"),1;
 g_gif.tex_cbp=32;g_gif.tex_cpsm=2;g_gif.tex_csa=0;g_gif.texa_ta1=0x80;
 gs_mem_write_psmct16(32*64u,64,1,0,0x801f);
 gs_load_clut(0x14,32,2,0,0,1);
 if(gs_sample_clut(1)!=0x800000f8)return puts("FAIL native 16-bit palette and TEXA alpha"),1;
 puts("PASS native indexed columns, shared high bits, page permutations and packed IMAGE upload");return 0;
}
