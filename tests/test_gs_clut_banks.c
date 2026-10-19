#include <assert.h>
#include <stdio.h>
#include "hw/gif.c"
int main(void){
 const unsigned formats[]={0,2,10};
 for(unsigned f=0;f<3;f++){unsigned cpsm=formats[f];
  gif_init();gs_mem_init();
  for(unsigned i=0;i<16;i++){
   if(cpsm){
    if(cpsm==10){gs_mem_write_psmct16s(0,64,i,0,0x801f);gs_mem_write_psmct16s(0,64,i,1,0x83e0);}
    else{gs_mem_write_psmct16(0,64,i,0,0x801f);gs_mem_write_psmct16(0,64,i,1,0x83e0);}
   }
   else{gs_mem_write_psmct32(0,64,i,0,0x800000ff);gs_mem_write_psmct32(0,64,i,1,0x8000ff00);}
  }
  g_gif.texclut_cbw=1;g_gif.texclut_cov=0;
  gs_load_clut(0x14,0,cpsm,1,2,1);
  g_gif.texclut_cov=1;gs_load_clut(0x14,0,cpsm,1,3,1);
  g_gif.tex_cpsm=cpsm;g_gif.tex_csm=1;g_gif.tex_csa=2;
  g_gif.texa_ta1=0x80;
  for(unsigned i=0;i<16;i++)assert(gs_sample_clut(i)==(cpsm?0x800000f8:0x800000ff));
  g_gif.tex_csa=3;for(unsigned i=0;i<16;i++)assert(gs_sample_clut(i)==(cpsm?0x8000f800:0x8000ff00));
 }
 puts("PASS CSM2 CSA bank retention and palette reuse for CT32/CT16/CT16S");return 0;
}
