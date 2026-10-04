/* Independent native-pixel coverage oracle for fractional flat SPRITEs. */
#include <stdio.h>
#include <math.h>
#include "hw/gif.c"
static void setup(void){gs_mem_init();gif_init();apply_ad_write(GS_REG_FRAME_1,2u<<16,0);apply_ad_write(GS_REG_SCISSOR_1,127u<<16,63u<<16);apply_ad_write(GS_REG_XYOFFSET_1,32768,32768);apply_ad_write(GS_REG_PRIM,6,0);apply_ad_write(GS_REG_RGBAQ,0x80868686,0);}
static void vertex(int x,int y){apply_xyz2_kick((unsigned)(32768+x),(unsigned)(32768+y),0,1);}
int main(void){
 unsigned cases=0;
 for(int phase=0;phase<16;phase++)for(int reverse=0;reverse<2;reverse++)for(int neg=0;neg<2;neg++){
  setup();int a=(neg?-2:2)*16+phase,b=18*16+phase,c=(neg?-1:1)*16+phase,d=11*16+phase;
  vertex(reverse?b:a,reverse?d:c);vertex(reverse?a:b,reverse?c:d);
  int left=(int)ceil(a/16.0),right=(int)ceil(b/16.0),top=(int)ceil(c/16.0),bottom=(int)ceil(d/16.0);
  for(int y=0;y<64;y++)for(int x=0;x<128;x++){
   unsigned expected=(x>=left&&x<right&&y>=top&&y<bottom)?0x80868686:0;
   if(gs_mem_read_psmct32(0,128,x,y)!=expected){printf("FAIL phase=%d reverse=%d neg=%d pixel=%d/%d\n",phase,reverse,neg,x,y);return 1;}
  }cases++;
 }
 setup();for(int n=0;n<2;n++){vertex(n*1024,0);vertex(n*1024+1023,63*16+15);}
 for(int y=0;y<64;y++)for(int x=0;x<128;x++)if(gs_mem_read_psmct32(0,128,x,y)!=0x80868686){printf("FAIL adjacent strips %d/%d\n",x,y);return 1;}
 /* Textured coverage and interpolation against an independent gradient.
  * Compare UV and STQ, either vertex order, all fractional phases. */
 for(int phase=0;phase<16;phase++)for(int reverse=0;reverse<2;reverse++)for(int fst=0;fst<2;fst++){
  setup();apply_ad_write(GS_REG_TEX0_1,8192u|(1u<<14)|(5u<<26)|(1u<<30),1u|(1u<<3));
  apply_ad_write(GS_REG_PRIM,6u|PRIM_TME_MASK|(fst?PRIM_FST_MASK:0),0);
  for(int y=0;y<32;y++)for(int x=0;x<32;x++)gs_mem_write_psmct32_blk(8192,64,x,y,0x80000000u|x|(y<<8));
  int a=2*16+phase,b=18*16+phase,c=1*16+phase,d=11*16+phase;
  for(int k=0;k<2;k++){
   int end=k^reverse;
   g_gif.cur_u=end?16:0;g_gif.cur_v=end?10:0;
   g_gif.cur_s=end?0.5f:0.0f;g_gif.cur_t=end?0.3125f:0.0f;g_gif.cur_q=1;
   vertex(end?b:a,end?d:c);
  }
  for(int y=0;y<64;y++)for(int x=0;x<128;x++){
   unsigned expected=0;
   if(x>=(int)ceil(a/16.0)&&x<(int)ceil(b/16.0)&&y>=(int)ceil(c/16.0)&&y<(int)ceil(d/16.0)){
    unsigned u=(unsigned)((x-a/16.0)*16.0/((b-a)/16.0)+0.5);
    unsigned v=(unsigned)((y-c/16.0)*10.0/((d-c)/16.0)+0.5);
    expected=0x80000000u|u|(v<<8); /* DECAL without TCC keeps vertex alpha */
   }
   if(gs_mem_read_psmct32(0,128,x,y)!=expected){printf("FAIL textured phase=%d reverse=%d fst=%d pixel=%d/%d got=%08x expected=%08x\n",phase,reverse,fst,x,y,gs_mem_read_psmct32(0,128,x,y),expected);return 1;}
  }cases++;
 }
 printf("PASS %u fractional coverage cases, flat/UV/STQ, reversed/negative coordinates and adjacent 64-pixel strips\n",cases);return 0;
}
