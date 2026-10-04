/* Public synthetic textures/depth. Compare resolve against the scalar GS;
 * actual EFB rasterization/TEV is tested separately, never claimed here. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "hw/gif.c"
static unsigned failures,cases;
#define CHECK(c,m) do{if(!(c)){printf("FAIL: %s case=%u\n",m,cases);failures++;}}while(0)
static uint32_t sample(int32_t x,int32_t y){return ((uint32_t)((x*31+y*17)&255)<<24)|((x*19+y*43)&255)|(((x*47+y*11)&255)<<8)|(((x*7+y*23)&255)<<16);}
static unsigned tile(unsigned w,unsigned x,unsigned y){return ((y/4)*(w/4)+x/4)*64+(y%4)*8+(x%4)*2;}
int main(void){
 uint8_t *initial=malloc(GS_MEM_SIZE),*expected=malloc(GS_MEM_SIZE),*actual=malloc(GS_MEM_SIZE);CHECK(initial&&expected&&actual,"allocate fixture");if(failures)return 1;
 int32_t col[16],row[16];gs_gx_texture_draw d;uint8_t source[4096],pixels[4096];
 for(unsigned mode=0;mode<4;mode++){
  for(unsigned i=0;i<16;i++){col[i]=mode==1?7-i/2:mode==2?3:(int)i/2;row[i]=i/2;}
  CHECK(gs_gx_prepare_texture(&d,0,0,64,2,3,16,16,col,row,mode==1?-0.5:mode==2?0:0.5,0.5,0),"prepare exact ramp");
  for(unsigned i=0;i<16;i++)CHECK((int)floor(((double)d.s0+((double)d.s1-d.s0)*(i+0.5)/16)*d.tw)+d.origin_x==col[i],"GPU center maps exact GS column");
 }
 for(unsigned i=0;i<16;i++){col[i]=i/2;row[i]=i/2;}
 CHECK(gs_gx_prepare_texture(&d,0,0,64,2,3,16,16,col,row,0.5,0.5,0),"base preparation");
 CHECK(gs_gx_pack_texture(source,sizeof(source),&d,sample)==d.tw*d.th*4,"exact un-clamped texture tiles");
 for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++){unsigned t=tile(16,x,y);uint32_t v=sample(col[x],row[y]);pixels[t]=255;pixels[t+1]=v;pixels[t+32]=v>>8;pixels[t+33]=v>>16;}
 unsigned coeffs[]={0,1,63,64,127,128,129,192,255};
 for(unsigned n=0;n<324;n++) {
  cases++;gif_init();gs_mem_init();unsigned psm=n%2,zpsms[]={0,1,2,10},zpsm=zpsms[(n/2)%4],ztst=(n/8)%4;
  unsigned a=(n/4)%3,b=(n/12)%3,dd=(n/36)%3,c=(n/3)%3,fix=coeffs[(n/9)%9],scan=(n/81)%4;
  CHECK(gs_gx_prepare_texture(&d,psm,0,64,2,3,16,16,col,row,0.5,0.5,scan),"prepare pipeline span");
  d.pipeline=(gs_gx_pipeline){16384,zpsm,1,1,ztst,n%3==0?0x80000080u:128,1,a,b,c,dd,fix,(n/7)%2,(n/5)%2};
  g_gif.fbp=0;g_gif.fbw=64;g_gif.frame_psm=psm;g_gif.prim=6|PRIM_ABE_MASK;g_gif.prmodecont_ac=1;g_gif.zbp=16384;g_gif.zpsm=zpsm;g_gif.zbuf_configured=1;g_gif.zmsk=0;g_gif.zte=1;g_gif.ztst=ztst;
  g_gif.alpha_a=a;g_gif.alpha_b=b;g_gif.alpha_c=c;g_gif.alpha_d=dd;g_gif.alpha_fix=fix;g_gif.pabe=d.pipeline.pabe;g_gif.colclamp=d.pipeline.colclamp;g_gif.colclamp_configured=1;g_gif.scanmsk=scan;
  for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++){gs_mem_write_psmct32(0,64,x+2,y+3,0xab123456u+x*0x00050203u+y*0x00030705u);gs_mem_write_z(16384,64,x+2,y+3,zpsm,(x+y)%3==0?127:(x+y)%3==1?128:129);}
  memcpy(initial,gs_mem_get(),GS_MEM_SIZE);
  for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++) {
   uint32_t z=gs_mem_read_z(16384,64,x+2,y+3,zpsm),fz=d.pipeline.z;
   int pass=ztst==1||(ztst==2&&fz>=z)||(ztst==3&&fz>z);
   if(pass)gs_finish_pixel(x+2,y+3,sample(col[x],row[y]),fz,1);
  }
  memcpy(expected,gs_mem_get(),GS_MEM_SIZE);memcpy(actual,initial,GS_MEM_SIZE);
  CHECK(gs_gx_import_texture(actual,GS_MEM_SIZE,pixels,sizeof(pixels),source,sizeof(source),&d),"resolve pipeline");
  CHECK(!memcmp(actual,expected,GS_MEM_SIZE),"full VRAM parity: depth masks/equality/Z formats, ALPHA/PABE/clamp, CT24 alpha/SCANMSK");
 }
 memcpy(actual,initial,GS_MEM_SIZE);d.column[0]=5000;
 CHECK(!gs_gx_import_texture(actual,GS_MEM_SIZE,pixels,sizeof(pixels),source,sizeof(source),&d),"invalid texture metadata rejected");
 CHECK(!memcmp(actual,initial,GS_MEM_SIZE),"invalid import is atomic");
 for(unsigned i=0;i<16;i++)col[i]=i%2?40:0;
 CHECK(!gs_gx_prepare_texture(&d,0,0,64,0,0,16,16,col,row,0.5,0.5,0),"discontinuous ramp falls back");
 CHECK(gs_blend_shift7(-1)==-1&&gs_blend_shift7(-129)==-2&&gs_blend_shift7(127)==0,"signed GS blend multiplication floors negative fractions");
 free(initial);free(expected);free(actual);printf("GX pipeline: %u cases, %u failures\n",cases,failures);return !!failures;
}
