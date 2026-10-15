/* GS blend/dither reference arithmetic; no BIOS/game/GPU input. */
#include <stdio.h>
#include <stdint.h>
#include "hw/gif.c"
static unsigned checks,failures;
static uint32_t seed=0x50004;
static uint32_t rnd(void){seed=seed*1664525u+1013904223u;return seed;}
static int div128(int n){int q=n/128;return q-(n<0&&n%128!=0);}
static unsigned channel(int n,unsigned clamp)
{return clamp?(n<0?0u:n>255?255u:(unsigned)n):((unsigned)n&255u);}
int main(void)
{
 const unsigned formats[]={0,1,2,10};
 gs_mem_init();gif_init();
 for(unsigned f=0;f<4;f++)for(unsigned n=0;n<8192;n++){
  unsigned psm=formats[f],is16=psm==2||psm==10,x=n&15,y=(n>>4)&15;
  uint32_t src=rnd(),old=rnd(),mask=rnd()&rnd();
  unsigned a=rnd()%3,b=rnd()%3,c=rnd()%3,d=rnd()%3,fix=rnd()&255;
  unsigned clamp=rnd()>>31,dthe=rnd()>>31,abe=rnd()>>31,pabe=rnd()>>31;
  int dith=(int)(rnd()&7)-4;
  g_gif.frame_psm=psm;g_gif.fbp=0;g_gif.fbw=64;g_gif.fbmask=mask;
  g_gif.prim=PRIM_TYPE_SPRITE|(abe?PRIM_ABE_MASK:0);g_gif.prmodecont_ac=1;
  g_gif.alpha_a=a;g_gif.alpha_b=b;g_gif.alpha_c=c;g_gif.alpha_d=d;g_gif.alpha_fix=fix;
  g_gif.colclamp_configured=1;g_gif.colclamp=clamp;g_gif.dthe=dthe;g_gif.pabe=pabe;
  g_gif.dimx[y&3][x&3]=dith;g_gif.fba=0;g_gif.ate=g_gif.date=0;
  uint32_t dest;
  if(is16){
   old&=65535;dest=((old&31)<<3)|(((old>>5)&31)<<11)|(((old>>10)&31)<<19)|((old&0x8000)?0x80000000u:0);
   (psm==10?gs_mem_write_psmct16s:gs_mem_write_psmct16)(0,64,x,y,old);
  }else{dest=psm==1?(old&0xffffff)|0x80000000u:old;gs_mem_write_psmct32(0,64,x,y,old);}
  uint32_t expected=src&0xff000000u;
  for(unsigned k=0;k<3;k++){
   int s=(src>>(k*8))&255,t=(dest>>(k*8))&255,v=s;
   if(abe&&(!pabe||(src>>31))){
    int values[3]={s,t,0};unsigned coeff=c==0?src>>24:c==1?dest>>24:fix;
    v=div128((values[a]-values[b])*(int)coeff)+values[d];
   }
   if(is16&&dthe)v+=dith;
   expected|=channel(v,clamp)<<(k*8);
  }
  if(is16){
   expected=((expected>>3)&31)|(((expected>>11)&31)<<5)|(((expected>>19)&31)<<10)|((expected>>16)&0x8000);
   unsigned shift=(gs_mem_framebuffer16_offset(0,64,x,y,psm)&2)?16:0;
   mask=(mask>>shift)&65535;
  }else if(psm==1)mask|=0xff000000u;
  expected=(expected&~mask)|(old&mask);
  gs_finish_pixel(x,y,src,0,0);
  uint32_t actual=is16?(psm==10?gs_mem_read_psmct16s:gs_mem_read_psmct16)(0,64,x,y):gs_mem_read_psmct32(0,64,x,y);
  checks++;if(actual!=expected){if(failures<8)printf("FAIL psm=%u n=%u actual=%08x expected=%08x\n",psm,n,actual,expected);failures++;}
 }
 printf("GS framebuffer blend/dither oracle: %u cases, %u failures\n",checks,failures);
 return failures?1:0;
}
