#include <assert.h>
#include <stdio.h>
#include "hw/gif.c"
int main(void){unsigned checks=0;gs_mem_init();
 for(unsigned packed=0;packed<2;packed++)for(unsigned f=0;f<16;f++){
  gif_init();apply_ad_write(GS_REG_FRAME_1,1u<<16,0); /* bp=0,bw=64 */
  apply_ad_write(GS_REG_TEX0_1,128u|(1u<<14)|(2u<<26)|(2u<<30),(1u<<2)|(1u<<3)); /* tbp128,4x4,DECAL */
  apply_ad_write(GS_REG_PRIM,PRIM_TYPE_TRIANGLE|PRIM_TME_MASK|PRIM_FST_MASK,0);
  apply_ad_write(GS_REG_TEX1_1,(1u<<5)|(1u<<6),0);
  unsigned raw=16u+f;
  if(packed){uint8_t data[16]={0};for(unsigned i=0;i<4;i++){data[i]=(raw|0xc000u)>>(i*8);data[4+i]=8u>>(i*8);}apply_packed_register(GS_REG_UV,data);}
  else apply_ad_write(GS_REG_UV,(raw|0xc000u)|(8u<<16),0);
  assert(g_gif.cur_u==raw/16.0&&g_gif.cur_v==0.5);
  for(unsigned y=0;y<4;y++)for(unsigned x=0;x<4;x++)gs_mem_write_psmct32(128*64,64,x,y,0x80000000u|x*64u);
  unsigned frac=f<8?f+8:f-8,left=f<8?0:64,right=f<8?64:128;
  uint32_t expected=0x80000000u|((left*(16-frac)+right*frac)>>4);
  double u=g_gif.cur_u,v=g_gif.cur_v;
  rasterize_triangle(0,0,4,0,0,4,0x80808080,0x80808080,0x80808080,u,v,u,v,u,v,0,0,1,0,0,1,0,0,1,0,0,0,0,0,0);
  assert(gs_mem_read_psmct32(0,64,1,1)==expected);
  rasterize_sprite_12_4_profile_impl(0,0,64,64,u,v,u,v,0,0,1,0,0,1,0,0,0,0);
  assert(gs_mem_read_psmct32(0,64,1,1)==expected);
  int32_t xy[6]={0,0,4,0,0,4};memcpy(gx_triangle.xy,xy,sizeof xy);gx_triangle.maxx=gx_triangle.maxy=4;gx_triangle.inv=1.0/edge(0,0,4,0,0,4);
  for(unsigned k=0;k<3;k++){gx_triangle.uv[k*2]=u;gx_triangle.uv[k*2+1]=v;gx_triangle.colors[k]=0x80808080;}
  assert(gs_gx_triangle_sample(1,1)==expected);checks++;
 }
 printf("PASS %u UV fractional decode/reserved-bit cases with triangle, sprite and GX-source bilinear output\n",checks);
}
