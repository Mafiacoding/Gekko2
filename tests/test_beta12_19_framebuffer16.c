/* Synthetic GS raster tests; no BIOS/ISO or hardware dependencies. */
#include <stdio.h>
#include <stdint.h>
#include "hw/gif.c"
static unsigned checks,failures;
#define CHECK(c) do{checks++;if(!(c)){fprintf(stderr,"FAIL %u %s\n",__LINE__,#c);failures++;}}while(0)
static uint16_t read16(unsigned psm,unsigned x,unsigned y)
{return (psm==10u?gs_mem_read_psmct16s:gs_mem_read_psmct16)(0,64,x,y);}
static void write16(unsigned psm,unsigned x,unsigned y,uint16_t val)
{if(psm==10u)gs_mem_write_psmct16s(0,64,x,y,val);else gs_mem_write_psmct16(0,64,x,y,val);}
int main(void)
{
  for(unsigned idx=0;idx<2;idx++) {
    unsigned psm=idx?10u:2u;
    gif_init();gs_mem_init();
    g_gif.fbp=0;g_gif.fbw=64;g_gif.frame_psm=psm;
    g_gif.prmodecont_ac=1;g_gif.prim=6;
    unsigned x=12,y=6;
    write16(psm,x,y,0x001fu);
    gs_finish_pixel(x,y,0x00112233u,0,0);
    CHECK(read16(psm,x,y)==(uint16_t)((2u<<10)|(4u<<5)|6u));
    g_gif.fba=1;
    gs_finish_pixel(x,y,0x00112233u,0,0);
    CHECK(read16(psm,x,y)==(uint16_t)(0x8000u|(2u<<10)|(4u<<5)|6u));
    g_gif.fba=0;
    /* Mask both physical halves independently; the GS swizzle is not x&1. */
    unsigned shift=(gs_mem_framebuffer16_offset(0,64,x,y,psm)&2u)?16u:0u;
    g_gif.fbmask=(uint32_t)0xffffu<<shift;
    write16(psm,x,y,0x1234u);
    gs_finish_pixel(x,y,0xff123456u,0,0);
    CHECK(read16(psm,x,y)==0x1234u);
    g_gif.fbmask=0xffffffffu^((uint32_t)0xffffu<<shift);
    gs_finish_pixel(x,y,0x80112233u,0,0);
    CHECK(read16(psm,x,y)==(uint16_t)(0x8000u|(2u<<10)|(4u<<5)|6u));
    g_gif.fbmask=0;g_gif.date=1;g_gif.datm=0;
    write16(psm,x,y,0x8001u);
    gs_finish_pixel(x,y,0x00112233u,0,0);
    CHECK(read16(psm,x,y)==0x8001u);
    g_gif.datm=1;
    gs_finish_pixel(x,y,0x00112233u,0,0);
    CHECK(read16(psm,x,y)!=(uint16_t)0x8001u);
  }
  gif_init();gs_mem_init();
  g_gif.fbp=0;g_gif.fbw=64;g_gif.frame_psm=0;
  g_gif.zbp=16384;g_gif.zpsm=2;
  g_gif.ate=1;g_gif.atst=0;g_gif.afail=2; /* NEVER and ZB_ONLY */
  gs_finish_pixel(4,5,0xffffffffu,0xfffff123u,1);
  CHECK(gs_mem_read_z(16384,64,4,5,2)==0xffffu);
  CHECK(gs_gx_framebuffer_store(0,0,0,1,2)==0x8000u);
  CHECK(gs_gx_framebuffer_store(0,0,0,1,10)==0x8000u);
  CHECK(gs_gx_framebuffer_store(0xffffu,0xabcd,0xffffu,1,2)==0xabcdu);
  printf("BETA12.19 GS CT16/16S FBA/FBMSK/DATE/Z: %u checks, %u failures\n",checks,failures);
  return !!failures;
}
