#include <stdio.h>
#include <string.h>
#include "hw/gs_mem.c"
#include "hw/gif.c"
static void w32(unsigned char*p,unsigned x){for(unsigned k=0;k<4;k++)p[k]=x>>(k*8);}
static unsigned failures;
#define CHECK(c,s) do{if(!(c)){puts("FAIL: " s);failures++;}}while(0)
static void init(void){gs_mem_init();gif_init();apply_ad_write(GS_REG_FRAME_1,10u<<16,0);}
int main(void){
 unsigned char b[112]={0};w32(b,2u|(1u<<15));w32(b+4,(1u<<14)|(PRIM_TYPE_SPRITE<<15)|(3u<<28));w32(b+8,GIF_REG_RGBAQ|(GS_REG_UV<<4)|(GIF_REG_XYZ2<<8));
 for(unsigned v=0;v<2;v++){
  unsigned char*p=b+16+v*48;w32(p,16);w32(p+4,32);w32(p+8,64);w32(p+12,128);
  w32(p+16,v*16);w32(p+20,v*16);w32(p+32,v*32);w32(p+36,v*32);
 }
 for(unsigned split=1;split<7;split++){
  init();gif_process_quadwords(GIF_PATH_3,b,split);gif_process_quadwords(GIF_PATH_3,b+split*16,7-split);
  CHECK(gs_mem_read_psmct32(0,640,0,0)==0x80402010u,"split PACKED sprite retains register sequence");
  CHECK(g_gif.sprites_drawn==1,"split PACKED draws one sprite");CHECK(g_gif.register_carry_path[GIF_PATH_3].remaining==0,"PACKED payload completes");
 }
 /* One register per loop, three values: high half of final qword is padding. */
 unsigned char r[48]={0};w32(r,3u|(1u<<15));w32(r+4,(1u<<26)|(1u<<28));w32(r+8,GIF_REG_RGBAQ);
 w32(r+16,0x80112233);w32(r+20,0x3f800000);w32(r+24,0x80445566);w32(r+28,0x3f800000);w32(r+32,0x80778899);w32(r+36,0x3f800000);w32(r+40,0xffffffff);
 init();gif_process_quadwords(GIF_PATH_3,r,1);gif_process_quadwords(GIF_PATH_3,r+16,1);CHECK(g_gif.rgba==0x80445566,"REGLIST continuation first two values");gif_process_quadwords(GIF_PATH_3,r+32,1);CHECK(g_gif.rgba==0x80778899,"REGLIST odd padding ignored");
 /* Other paths may issue packets while PATH3's payload is unfinished. */
 init();gif_process_quadwords(GIF_PATH_3,b,1);gif_process_quadwords(GIF_PATH_2,r,3);CHECK(g_gif.register_carry_path[GIF_PATH_3].remaining==6,"PATH2 does not consume PATH3 payload");gif_process_quadwords(GIF_PATH_3,b+16,6);CHECK(g_gif.sprites_drawn==1,"PATH3 resumes after PATH2");
 /* PATH1's EOP takes effect at the payload end, including split delivery. */
 init();gif_process_quadwords(GIF_PATH_1,b,1);gif_process_quadwords(GIF_PATH_1,b+16,6);CHECK(g_gif.sprites_drawn==1,"PATH1 split EOP finishes payload");
 puts(failures?"FAIL: GIF register carry":"PASS: PACKED/REGLIST splits, odd padding, path isolation and EOP");return failures!=0;
}
