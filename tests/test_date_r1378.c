#include <assert.h>
#include <stdio.h>
#include "hw/gif.c"
int main(void){
 for(unsigned datm=0;datm<2;datm++)for(unsigned bit=0;bit<2;bit++){
  gif_init();gs_mem_init();g_gif.fbw=64;g_gif.zbp=16384;g_gif.zpsm=0;
  g_gif.date=1;g_gif.datm=datm;
  uint32_t old=0x00123456|(bit<<31);
  gs_mem_write_psmct32(0,64,3,4,old);gs_mem_write_z(16384,64,3,4,0,7);
  gs_finish_pixel(3,4,0x80abcdef,99,1);
  assert(gs_mem_read_psmct32(0,64,3,4)==(bit==datm?0x80abcdef:old));
  assert(gs_mem_read_z(16384,64,3,4,0)==(bit==datm?99:7));
 }
 for(unsigned datm=0;datm<2;datm++){
  gif_init();gs_mem_init();g_gif.fbw=64;g_gif.frame_psm=1;g_gif.date=1;g_gif.datm=datm;
  gs_mem_write_psmct32(0,64,3,4,0x80123456);gs_finish_pixel(3,4,0x80abcdef,99,0);
  assert(gs_mem_read_psmct32(0,64,3,4)==0x80123456);
 }
 for(unsigned psm=2;psm<=10;psm+=8)for(unsigned bit=0;bit<2;bit++){
  gif_init();gs_mem_init();g_gif.fbw=64;g_gif.frame_psm=psm;g_gif.date=1;g_gif.datm=!bit;
  (psm==10?gs_mem_write_psmct16s:gs_mem_write_psmct16)(0,64,3,4,0x1234|(bit<<15));
  gs_finish_pixel(3,4,0x80abcdef,99,0);
  assert((psm==10?gs_mem_read_psmct16s:gs_mem_read_psmct16)(0,64,3,4)==(0x1234|(bit<<15)));
 }
 gif_init();apply_ad_write(GS_REG_TEST_1,1u<<14,0);apply_ad_write(GS_REG_TEST_2,3u<<14,0);
 g_gif.prmodecont_ac=1;g_gif.prim=3;gs_activate_context();assert(g_gif.date&&!g_gif.datm&&!g_gx_opaque&&!g_opaque_pixel);
 g_gif.prim=3|(1u<<9);gs_activate_context();assert(g_gif.date&&g_gif.datm);
 gif_init();gs_mem_init();g_gif.tex_tbw=64;g_gif.tex_tw=2;g_gif.tex_th=2;
 g_gif.clamp_configured=1;g_gif.clamp_wms=GS_CLAMP_REPEAT;g_gif.clamp_wmt=GS_CLAMP_REPEAT;
 for(unsigned y=0;y<4;y++)for(unsigned x=0;x<4;x++)gs_mem_write_psmct32(0,64,x,y,0x80000000|(y*4+x+1));
 assert(gs_sample_texture(0.75,0.75)==0x80000001);
 assert(gs_sample_texture(1.75,2.75)==0x8000000a);
 assert(gs_sample_texture(-0.25,-0.25)==0x80000010);
 int32_t map[3];unsigned phase;assert(gs_gx_sprite_axis(map,3,-0.25,1.0,0,&phase,NULL));
 assert(map[0]==-1&&map[1]==0&&map[2]==1);
 puts("PASS nearest fractional texel selection and negative REPEAT coordinates");
 puts("PASS DATE CT32 DATM0/1 alpha-bit pass/reject, rejected Z unchanged, CT24 rejection, CT16/16S rejection, both contexts, opaque fast-path disabled");return 0;
}
