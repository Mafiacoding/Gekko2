/* All legal GS texture views -> GX RGBA8, plus exact late framebuffer writes. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hw/gif.c"
#include "core/hw/gs_wii_output.h"
static unsigned tile(unsigned w,unsigned x,unsigned y){return ((y/4)*(w/4)+x/4)*64+(y&3)*8+(x&3)*2;}
static uint32_t sample(int32_t x,int32_t y){return 0x11223344u+(uint32_t)x*0x19372715u+(uint32_t)y*0x37296113u;}
static uint32_t seed=0x50004;
static uint32_t rnd(void){seed=seed*1664525u+1013904223u;return seed;}
int main(void){
 const unsigned formats[]={0,1,2,10,0x13,0x14,0x1b,0x24,0x2c,0x30,0x31,0x32,0x3a};
 /* Independent hardware rule: RGB_ONLY preserves the single CT16 alpha bit,
  * even if source alpha or FBA would otherwise change it. */
 for(unsigned f=0;f<2;f++)for(unsigned old=0;old<2;old++)for(unsigned fba=0;fba<2;fba++){
  gif_init();gs_mem_init();g_gif.frame_psm=f?10:2;g_gif.fbw=64;g_gif.prim=6;g_gif.prmodecont_ac=1;
  g_gif.ate=1;g_gif.atst=0;g_gif.afail=3;g_gif.fba=fba;
  (f?gs_mem_write_psmct16s:gs_mem_write_psmct16)(0,64,0,0,old<<15);
  gs_finish_pixel(0,0,((1u-old)<<31)|0x00123456u,0,0);
  assert(((f?gs_mem_read_psmct16s:gs_mem_read_psmct16)(0,64,0,0)>>15)==old);
 }
 unsigned packed=0;uint8_t upload[256];gs_gx_texture_draw d={0};d.tw=d.th=8;
 for(unsigned f=0;f<13;f++){
  gif_init();gs_mem_init();unsigned psm=formats[f];g_gif.tex_psm=psm;g_gif.tex_tbp0=128;g_gif.tex_tbw=128;
  g_gif.texa_ta0=64;g_gif.texa_ta1=128;g_gif.texa_aem=1;g_gif.tex_cpsm=0;
  for(unsigned i=0;i<256;i++)g_gif.clut_cache[i]=0x80000000u+i*0x010307u;
  uint32_t expected[64];
  for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++){
   unsigned n=y*8+x;uint32_t v=n?sample(x,y):0,raw=v;int is16=psm==2||psm==10||psm==0x32||psm==0x3a;
   if(is16)raw&=65535;
   if(psm>=0x30)gs_mem_write_z(128*64,128,x,y,psm&15,raw);
   else if(is16)(psm==10?gs_mem_write_psmct16s:gs_mem_write_psmct16)(128*64,128,x,y,raw);
   else if(psm==0||psm==1)gs_mem_write_psmct32(128*64,128,x,y,raw);
   else{unsigned index=(psm==0x13||psm==0x1b)?n:n&15;gs_mem_write_index(128*64,128,x,y,psm,index);raw=g_gif.clut_cache[index];}
   v=raw;
   if(psm==1||psm==0x31)v=(raw&0xffffff)|((raw&0xffffff?64u:0u)<<24);
   if(is16)v=((raw&31)<<3)|(((raw>>5)&31)<<11)|(((raw>>10)&31)<<19)|((raw&0x8000?128u:raw&0x7fff?64u:0u)<<24);
   expected[n]=v;
  }
  assert(gs_gx_pack_texture(upload,sizeof upload,&d,gs_sample_texel_uncached)==sizeof upload);
  for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++){
   unsigned t=tile(8,x,y);uint32_t v=expected[y*8+x];
   assert(upload[t]==v>>24&&upload[t+1]==(uint8_t)v&&upload[t+32]==(uint8_t)(v>>8)&&upload[t+33]==(uint8_t)(v>>16));packed++;
  }
 }
 /* All legal DISPLAY formats also enter the GX RGBA8 scanout packer. */
 for(unsigned f=0;f<4;f++){
  unsigned psm=formats[f];gs_mem_init();
  for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++){
   uint32_t c=sample(x,y);
   if(psm==2||psm==10)(psm==10?gs_mem_write_psmct16s:gs_mem_write_psmct16)(0,64,x,y,c);
   else gs_mem_write_psmct32(0,64,x,y,c);
  }
  assert(gs_gx_pack_rgba8_psm(upload,sizeof upload,0,64,0,0,8,8,psm)==sizeof upload);
  for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++){
   uint32_t c=gs_scanout_read_rgb(0,64,x,y,psm);unsigned t=tile(8,x,y);
   assert(upload[t]==255&&upload[t+1]==(uint8_t)c&&upload[t+32]==(uint8_t)(c>>8)&&upload[t+33]==(uint8_t)(c>>16));
  }
 }
 assert(!gs_gx_pack_rgba8_psm(upload,sizeof upload,0,64,0,0,8,8,0x30)); /* DISPLAY has no Z PSM. */
 uint8_t *initial=malloc(GS_MEM_SIZE),*expected=malloc(GS_MEM_SIZE),*actual=malloc(GS_MEM_SIZE);assert(initial&&expected&&actual);
 int32_t col[8],row[8];for(unsigned i=0;i<8;i++)col[i]=row[i]=i;
 uint8_t source[256],pixels[256];unsigned targets[]={0,1,2,10};
 for(unsigned n=0;n<1024;n++){
  gif_init();gs_mem_init();unsigned psm=targets[n&3],is16=psm==2||psm==10,zpsm=targets[(n>>2)&3];
  unsigned dx=2+((n>>5)%7)*8,dy=3+((n>>8)%4)*24;
  assert(gs_gx_prepare_texture(&d,psm,0,64,dx,dy,8,8,col,row,1,1,(n>>4)%4));
  gs_gx_pipeline *p=&d.pipeline;
  p->zbp=16384;p->zpsm=zpsm;p->ztest=(rnd()>>11)&1;p->zwrite=(rnd()>>12)&1;p->ztst=(rnd()>>8)&3;p->z=rnd();
  p->blend=(rnd()>>8)&1;p->a=rnd()%3;p->b=rnd()%3;p->c=rnd()%3;p->d=rnd()%3;p->fix=rnd()&255;p->pabe=(rnd()>>8)&1;p->colclamp=(rnd()>>8)&1;
  p->fbmask=rnd()&rnd();p->fba=(rnd()>>8)&1;p->date=(rnd()>>8)&1;p->datm=(rnd()>>8)&1;
  p->ate=(rnd()>>8)&1;p->atst=(rnd()>>8)&7;p->aref=rnd()&255;p->afail=(rnd()>>8)&3;
  g_gif.fbp=0;g_gif.fbw=64;g_gif.frame_psm=psm;g_gif.fbmask=p->fbmask;g_gif.fba=p->fba;
  g_gif.prim=6|(p->blend?PRIM_ABE_MASK:0);g_gif.prmodecont_ac=1;g_gif.scanmsk=(n>>4)%4;
  g_gif.zbp=p->zbp;g_gif.zpsm=zpsm;g_gif.zbuf_configured=1;g_gif.zmsk=!p->zwrite;g_gif.zte=p->ztest;g_gif.ztst=p->ztst;
  g_gif.alpha_a=p->a;g_gif.alpha_b=p->b;g_gif.alpha_c=p->c;g_gif.alpha_d=p->d;g_gif.alpha_fix=p->fix;g_gif.pabe=p->pabe;
  g_gif.colclamp_configured=1;g_gif.colclamp=p->colclamp;g_gif.date=p->date;g_gif.datm=p->datm;g_gif.ate=p->ate;g_gif.atst=p->atst;g_gif.aref=p->aref;g_gif.afail=p->afail;
  assert(gs_gx_pack_texture(source,sizeof source,&d,sample)==sizeof source);memcpy(pixels,source,sizeof pixels);
  for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++){
   uint32_t v=rnd();if(is16)(psm==10?gs_mem_write_psmct16s:gs_mem_write_psmct16)(0,64,x+dx,y+dy,v);
   else gs_mem_write_psmct32(0,64,x+dx,y+dy,v);
   gs_mem_write_z(16384,64,x+dx,y+dy,zpsm,rnd());
  }
  memcpy(initial,gs_mem_get(),GS_MEM_SIZE);
  for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++){
   uint32_t incoming=gs_mem_clamp_raster_z(p->z,zpsm),z=gs_mem_read_z(16384,64,x+dx,y+dy,zpsm);
   if(!p->ztest||p->ztst==1||(p->ztst==2&&incoming>=z)||(p->ztst==3&&incoming>z))gs_finish_pixel(x+dx,y+dy,sample(x,y),p->z,p->zwrite);
  }
  memcpy(expected,gs_mem_get(),GS_MEM_SIZE);memcpy(actual,initial,GS_MEM_SIZE);
  assert(gs_gx_import_texture(actual,GS_MEM_SIZE,pixels,sizeof pixels,source,sizeof source,&d));
  if(memcmp(expected,actual,GS_MEM_SIZE)){unsigned off=0;while(expected[off]==actual[off])off++;printf("FAIL framebuffer case %u psm=%u off=%u expected=%02x actual=%02x blend=%u date=%u datm=%u ate=%u atst=%u afail=%u mask=%08x fba=%u zwrite=%u\n",n,psm,off,expected[off],actual[off],p->blend,p->date,p->datm,p->ate,p->atst,p->afail,p->fbmask,p->fba,p->zwrite);return 1;}
 }
 free(initial);free(expected);free(actual);
 printf("PASS 13 GS->GX source formats (%u texels), 1024 four-target VRAM/DATE/ATE/AFAIL/FBA/FBMSK/blend/Z comparisons\n",packed);return 0;
}
