#include "core/hw/gs_wii_output.h"
#include "core/hw/gs_mem.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t display(unsigned x,unsigned y,unsigned w,unsigned h)
{return x|((uint64_t)y<<12)|((uint64_t)(w-1)<<32)|((uint64_t)(h-1)<<44);}
static int failed(void *ctx,uint8_t *v,uint32_t n){(void)ctx;(void)v;(void)n;return 0;}
int main(void)
{
 gs_mem_init();gs_state_t g={0};g.pmode=3|32;g.dispfb1=1ull<<9;g.dispfb2=(1ull<<9)|4;
 g.display1=display(100,20,4,2);g.display2=display(102,21,4,2);g.bgcolor=0x00ff00;
 for(unsigned y=0;y<2;y++)for(unsigned x=0;x<4;x++){
  gs_mem_write_psmct32(0,64,x,y,0x800000ff);gs_mem_write_psmct32(8192,64,x,y,0x80ff0000);
 }
 gs_output_plan p;assert(gs_output_make_plan(&g,&p));assert(p.width==6&&p.height==3);
 assert(p.circuit[0].dx==0&&p.circuit[1].dx==2&&p.circuit[1].dy==1);
 /* All constant ALP values, independent red/blue expected pair. */
 for(unsigned alpha=0;alpha<256;alpha++){
  g.pmode=3|32|((uint64_t)alpha<<8);assert(gs_output_make_plan(&g,&p));
  uint32_t value=gs_output_read_rgba(&p,2,1);
  assert((value&255u)==alpha&&((value>>16)&255u)==255-alpha);
 }
 g.pmode=3|32|((uint64_t)128<<8);assert(gs_output_make_plan(&g,&p));
 assert(gs_output_read_rgba(&p,2,1)==0x807f0080u);
 assert((gs_output_read_rgba(&p,5,2)&0xffffff)==0xff0000u); /* only RC2 */
 assert((gs_output_read_rgba(&p,0,2)&0xffffff)==0x00ff00u); /* aperture background */
 g.pmode|=128;assert(gs_output_make_plan(&g,&p));assert((gs_output_read_rgba(&p,2,1)&0xffffff)==0x007f80u);
 g.pmode=3;assert(gs_output_make_plan(&g,&p));assert(gs_output_read_rgba(&p,2,1)==0x800000ffu);
 gs_mem_write_psmct32(0,64,2,1,0x400000ff);assert((gs_output_read_rgba(&p,2,1)&0xffffff)==0x7f0080u);
 g.pmode|=64;assert(gs_output_make_plan(&g,&p));assert((gs_output_read_rgba(&p,2,1)>>24)==128u);
 uint64_t key=gs_output_state_key(&g);g.display2++;assert(gs_output_state_key(&g)!=key);g.display2--;
 g.smode2=3;assert(gs_output_make_plan(&g,&p));assert(p.circuit[0].height==1&&p.circuit[1].height==1);
 g.smode2=0;g.pmode=2;assert(!gs_output_requires_merge(&g));
 g.pmode=1|32|(255ull<<8);assert(!gs_output_requires_merge(&g));
 g.pmode=3|32|(128ull<<8);assert(gs_output_has_rgb(&g));
 uint32_t xfb[9];memset(xfb,0x5a,sizeof xfb);assert(gs_output_present_checked(&g,xfb,6,3)==1);
 uint32_t before[9];memcpy(before,xfb,sizeof before);g.dispfb1|=3ull<<15;
 assert(gs_output_present_checked(&g,xfb,6,3)==0&&!memcmp(before,xfb,sizeof before));
 g.dispfb1&=~(31ull<<15);assert(gs_mem_gpu_bind(failed,0)&&gs_mem_gpu_mark_pending());
 assert(gs_output_present_checked(&g,xfb,6,3)==-1&&!memcmp(before,xfb,sizeof before));
 puts("PASS PCRTC merge: 256 ALP values, source alpha/AMOD, SLBG/BGCOLOR, two offsets, field height, state key and transactional invalid/fence failure");
}
