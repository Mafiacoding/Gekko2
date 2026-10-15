#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "hw/gif.c"
static void put(unsigned char *p,uint32_t v){p[0]=v;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}
static void tag(unsigned char *p,unsigned n,uint32_t regs){memset(p,0,16);put(p,1u|(1u<<15));put(p+4,n<<28);put(p+8,regs);}
int main(void)
{
 unsigned char p[80]={0};gif_init();
 assert(g_gif.cur_q==1 && g_gif.gif_q_bits==0x3f800000u);
 /* Two STQ writes with XYZ kicks must not change GS Q without RGBA. */
 tag(p,4,0x5252);put(p+16,0x3f400000);put(p+24,0x40000000);
 put(p+32,16);put(p+36,32);put(p+48,0x3f000000);put(p+56,0x40800000);
 put(p+64,48);put(p+68,64);
 gif_process_quadwords(GIF_PATH_3,p,5);
 assert(g_gif.gif_q_bits==0x40800000u && g_gif.cur_q==1);
 /* STQ and RGBA in one fragmented packet latch Q only at RGBA. */
 tag(p,3,0x512);put(p+16,0x3f400000);put(p+20,0x3f000000);put(p+24,0x40000000);
 memset(p+32,0,32);put(p+32,128);put(p+36,128);put(p+40,128);put(p+44,128);
 put(p+48,48);put(p+52,64);
 gif_process_quadwords(GIF_PATH_3,p,2);
 assert(g_gif.cur_q==1 && g_gif.gif_q_bits==0x40000000u);
 gif_process_quadwords(GIF_PATH_3,p+32,2);
 assert(g_gif.cur_q==2);
 /* A+D Q is independent of the temporary GIF latch. */
 apply_ad_write(GS_REG_RGBAQ,0x80808080,0x40400000);
 assert(g_gif.cur_q==3 && g_gif.gif_q_bits==0x40000000u);
 /* A new tag resets temporary Q to one, not the GS register. */
 tag(p,1,GIF_REG_XYZ3);memset(p+16,0,16);
 gif_process_quadwords(GIF_PATH_3,p,2);
 assert(g_gif.cur_q==3 && g_gif.gif_q_bits==0x3f800000u);
 /* NLOOP=0 disregards all fields except EOP. */
 g_gif.gif_q_bits=0x40800000;put(p,1u<<15);
 gif_process_quadwords(GIF_PATH_3,p,1);
 assert(g_gif.cur_q==3 && g_gif.gif_q_bits==0x40800000);
 puts("PASS independent GIF/GS Q, PACKED RGBA latching, fragments, A+D and empty tags");return 0;
}
