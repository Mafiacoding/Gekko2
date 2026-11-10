#include <assert.h>
#include <stdio.h>
#include "hw/gif.c"
int main(void){
 const unsigned formats[]={0x30,0x31,0x32,0x3a};unsigned checks=0;
 gif_init();gs_mem_init();
 for(unsigned f=0;f<4;f++)for(unsigned wrap=0;wrap<2;wrap++)
 for(unsigned a0=0;a0<3;a0++)for(unsigned a1=0;a1<2;a1++)for(unsigned aem=0;aem<2;aem++){
  unsigned psm=formats[f],view=psm&15,base=wrap?16383:128;
  g_gif.tex_tbp0=base;g_gif.tex_tbw=128;g_gif.tex_psm=psm;
  g_gif.texa_ta0=a0==0?0:a0==1?128:255;g_gif.texa_ta1=a1?128:64;g_gif.texa_aem=aem;
  for(unsigned n=0;n<64;n++){
   unsigned x=(n*17)%128,y=(n*9)%96;
   unsigned value=n%8==0?0:n%8==1?0x8000:n%8==2?0x7fff:n*0x01234567u;
   if(view==1)value&=0xffffff;if(view==2||view==10)value&=65535;
   gs_mem_write_z(base*64,128,x,y,view,value);
   unsigned expected=value;
   if(view==1)expected=value|((aem&&!value?0:g_gif.texa_ta0)<<24);
   if(view==2||view==10){
    unsigned alpha=(value&0x8000)?g_gif.texa_ta1:(aem&&!(value&0x7fff)?0:g_gif.texa_ta0);
    expected=((value&31)<<3)|(((value>>5)&31)<<11)|(((value>>10)&31)<<19)|(alpha<<24);
   }
   assert(gs_sample_texel_uncached(x,y)==expected);checks++;
  }
 }
 printf("PASS depth-texture PSMZ32/24/16/16S, TEXA/AEM, page and VRAM wrap: %u cases\n",checks);return 0;
}
