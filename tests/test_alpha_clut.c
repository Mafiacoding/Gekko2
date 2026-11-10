#include <assert.h>
#include <stdio.h>
#include "hw/gif.c"
static uint32_t expand(unsigned v)
{return ((v&31u)*8u)|(((v>>5)&31u)*8u<<8)|(((v>>10)&31u)*8u<<16)|((v&32768u)?0x80000000u:0);}
static void view(unsigned psm,unsigned cpsm,unsigned csm,unsigned csa)
{g_gif.tex_psm=psm;g_gif.tex_cpsm=cpsm;g_gif.tex_csm=csm;g_gif.tex_csa=csa;g_gif.texa_ta0=0;g_gif.texa_ta1=128;}
static uint32_t color(unsigned i){return (0x801fu+i)<<16|(0x0120u+i);}
int main(void)
{
 gif_init();gs_mem_init();g_gif.texclut_cbw=1;
 for(unsigned i=0;i<16;i++)gs_mem_write_psmct32(0,64,i,0,color(i));
 gs_load_clut(0x14,0,0,1,0,1);
 /* CT32 to two CT16 banks with no source reload. */
 view(0x14,2,1,0);for(unsigned i=0;i<16;i++)assert(gs_sample_clut(i)==expand(0x0120u+i));
 view(0x14,10,1,16);for(unsigned i=0;i<16;i++)assert(gs_sample_clut(i)==expand(0x801fu+i));
 /* Independently write the low/high CT16 banks, then reinterpret as CT32. */
 for(unsigned i=0;i<16;i++)gs_mem_write_psmct16(0,64,i,0,0x2340u+i);
 gs_load_clut(0x14,0,2,1,0,1);
 for(unsigned i=0;i<16;i++)gs_mem_write_psmct16s(0,64,i,0,0xab00u+i);
 gs_load_clut(0x14,0,10,1,16,1);
 view(0x14,0,1,0);for(unsigned i=0;i<16;i++)assert(gs_sample_clut(i)==((0xab00u+i)<<16|(0x2340u+i)));
 /* CSM1 I8: literal source arrangement, nonzero CSA and clamped bank. */
 gif_init();gs_mem_init();
 for(unsigned i=0;i<256;i++){
  unsigned x=(i%8)+((i/16)%2)*8,y=((i/8)%2)+(i/32)*2;
  gs_mem_write_psmct32(0,64,x,y,color(i));
 }
 gs_load_clut(0x13,0,0,0,0,1);view(0x13,0,0,0);
 for(unsigned i=0;i<256;i++)assert(gs_sample_clut(i)==color(i));
 for(unsigned csa=0;csa<16;csa++){
  gs_load_clut(0x13,0,0,0,csa,1);view(0x13,0,0,csa);
  for(unsigned i=0;i<256;i++){
   unsigned bank=csa+i/16;if(bank>15)bank=15;
   assert(gs_sample_clut(i)==color(bank*16+i%16));
  }
 }
 /* No reload while switching to CSM2 I4: second half has a distinct stride. */
 view(0x14,0,1,0);for(unsigned i=0;i<16;i++)assert(gs_sample_clut(i)==color(i<8?i:i+8));
 uint16_t snapshot[1024];memcpy(snapshot,g_gif.clut_words,sizeof snapshot);
 for(unsigned cld=0;cld<8;cld+=6){gs_load_clut(0x14,0,0,1,0,cld);assert(!memcmp(snapshot,g_gif.clut_words,sizeof snapshot));}
 gs_load_clut(0x14,0,0,1,0,7);assert(!memcmp(snapshot,g_gif.clut_words,sizeof snapshot));
 puts("PASS physical CLUT: CT32/16/16S cross-view, separate halves, 4096 I8 CSA/clamp samples, CSM reinterpretation and no-reload commands");
}
