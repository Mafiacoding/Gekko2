#include "hw/gif.c"
#include <stdio.h>
static unsigned char original[GS_MEM_SIZE],expected[GS_MEM_SIZE];
#define CHECK(v) do {if(!(v)){printf("FAIL %d: %s\n",__LINE__,#v);return 1;}}while(0)
static void configure(unsigned alias,unsigned linear,unsigned clamp,unsigned zwrite)
{
 gif_init();gs_mem_init();
 apply_ad_write(GS_REG_FRAME_1,1u<<16,0);
 apply_ad_write(GS_REG_XYOFFSET_1,0,0);
 apply_ad_write(GS_REG_SCISSOR_1,63u<<16,31u<<16);
 apply_ad_write(GS_REG_TEX0_1,(alias?0u:4096u)|(1u<<14)|(4u<<26),13u);
 if(clamp)apply_ad_write(GS_REG_CLAMP_1,0,0);
 apply_ad_write(GS_REG_TEX1_1,linear?0x60u:0u,0);
 apply_ad_write(GS_REG_ZBUF_1,alias?256u:128u,zwrite?0u:1u);
 apply_ad_write(GS_REG_TEST_1,(1u<<16)|(1u<<17),0); /* ALWAYS */
 apply_ad_write(GS_REG_PRIM,0x116u,0);
 apply_ad_write(GS_REG_RGBAQ,0x80808080u,0);
 for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++)
  gs_mem_write_psmct32(alias?0u:4096u*64u,64,x,y,0x80000000u|(x*11u)|((y*13u)<<8)|(((x+y)*7u)<<16));
}
int main(void)
{
 for(unsigned alias=0;alias<2;alias++)for(unsigned linear=0;linear<2;linear++)
 for(unsigned clamp=0;clamp<2;clamp++)for(unsigned zwrite=0;zwrite<2;zwrite++)for(unsigned reverse=0;reverse<2;reverse++) {
  configure(alias,linear,clamp,zwrite);gs_activate_context();
  memcpy(original,gs_mem_get(),GS_MEM_SIZE);
  int x0=reverse?64:0,x1=reverse?0:64;double u0=reverse?-2.0:0.0,u1=reverse?18.0:16.0;
  /* Independent scalar loop retains the previous per-pixel formula and
   * source-before-destination ordering, including framebuffer/Z alias. */
  g_texel_cache_enabled=0;
  for(int y=0;y<32;y++)for(int x=0;x<64;x++) {
   double fu=((double)x-(double)x0)/(double)(x1-x0),fv=(double)y/32.0;
   uint32_t color=gs_texture_function(gs_sample_texture(u0+fu*(u1-u0),fv*16.0),g_gif.rgba);
   gs_finish_draw_pixel(x,y,color,55u,g_gif.zbuf_configured&&!g_gif.zmsk);
  }
  memcpy(expected,gs_mem_get(),GS_MEM_SIZE);memcpy(gs_mem_get(),original,GS_MEM_SIZE);
  uint64_t before=gif_get_render_work(10);
  rasterize_sprite(x0,0,x1,32,(int)u0,0,(int)u1,16,0,0,1,0,0,1,0,55,255,255);
  CHECK(!memcmp(expected,gs_mem_get(),GS_MEM_SIZE));CHECK(!g_texel_cache_enabled);
  /* Source overlaps FRAME in alias mode and writable Z otherwise. */
  CHECK(gif_get_render_work(10)-before==(!alias&&clamp&&!zwrite));
 }
 for(unsigned config=0;config<2;config++)for(unsigned mask=0;mask<2;mask++)
 for(unsigned test=0;test<2;test++)for(unsigned func=0;func<4;func++) {
  g_gif.zbuf_configured=config;g_gif.zmsk=mask;g_gif.zte=test;g_gif.ztst=func;
  CHECK(gs_depth_is_inactive()==(!config||(mask&&(!test||func==GS_ZTST_ALWAYS))));
 }
 puts("PASS 32 full-VRAM sprite comparisons: cache, filter, coordinates, FRAME/Z alias, inactive-depth policy");return 0;
}
