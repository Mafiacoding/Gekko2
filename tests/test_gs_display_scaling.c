#include <stdio.h>
#include "core/hw/gs_wii_output.h"
#include "core/hw/gs_mem.h"
static unsigned blend(unsigned a,unsigned b,unsigned f){unsigned r=0;for(unsigned c=0;c<32;c+=8)r|=(((((a>>c)&255u)*(4-f)+((b>>c)&255u)*f+2)/4)&255u)<<c;return r;}
int main(void){unsigned x,y,w,h;gs_decode_display_region(0x1450ull|(13ull<<32)|(17ull<<48),0x1ff9ff01848290ull,3,&x,&y,&w,&h);
 if(x!=13||y!=17||w!=640||h!=256)return puts("FAIL PAL display viewport and field height"),1;
 gs_decode_display_region(0,0x1ff9ff01848290ull,1,&x,&y,&w,&h);if(h!=512)return puts("FAIL full frame height"),1;
 gs_mem_init();gs_mem_write_psmct32(0,64,5,7,0xff0000ff);gs_mem_write_psmct32(0,64,6,7,0xff0000ff);
 gs_mem_write_psmct32(0,64,5,8,0xff00ff00);gs_mem_write_psmct32(0,64,6,8,0xff00ff00);
 unsigned out[10];for(unsigned i=0;i<10;i++)out[i]=0xabcdef01;
 gs_blit_scaled_psmct32_to_xfb(out+1,4,4,0,64,5,7,2,2);
 unsigned r=gs_rgb8_pair_to_ycbcr(255,0,0,255,0,0),g=gs_rgb8_pair_to_ycbcr(0,255,0,0,255,0);
 if(out[0]!=0xabcdef01||out[9]!=0xabcdef01)return puts("FAIL output bounds"),1;
 unsigned expected[4]={r,blend(r,g,1),blend(r,g,3),g};
 for(unsigned i=1;i<=8;i++)if(out[i]!=expected[(i-1)/2])return puts("FAIL centered filtered field rows and source offset"),1;
 unsigned identity[2];gs_blit_scaled_psmct32_to_xfb(identity,2,2,0,64,5,7,2,2);
 if(identity[0]!=r||identity[1]!=g)return puts("FAIL identity scaling"),1;
 unsigned down;gs_blit_scaled_psmct32_to_xfb(&down,2,1,0,64,5,7,2,2);
 if(down!=blend(r,g,2))return puts("FAIL vertical downscale midpoint"),1;
 gs_blit_scaled_psmct32_to_xfb(out+1,3,4,0,64,5,7,2,2);
 if(out[1]!=r||out[9]!=0xabcdef01)return puts("FAIL odd-width rejection"),1;
 puts("PASS GS viewport, interlaced field and scaled Wii framebuffer");return 0;}
