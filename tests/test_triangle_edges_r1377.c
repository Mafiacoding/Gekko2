#include <assert.h>
#include <stdio.h>
#include "hw/gif.c"
static void draw(const int32_t *p){rasterize_triangle(p[0],p[1],p[2],p[3],p[4],p[5],0x80181818,0x80181818,0x80181818,0,0,0,0,0,0,0,0,1,0,0,1,0,0,1,0,0,0,255,255,255);}
int main(void){
 for(unsigned diagonal=0;diagonal<2;diagonal++)for(unsigned reverse=0;reverse<2;reverse++){
  int32_t a[6]={2,3,10,3,2,11},b[6]={10,3,10,11,2,11};
  if(diagonal){int32_t c[6]={2,3,10,3,10,11},d[6]={2,3,10,11,2,11};memcpy(a,c,sizeof a);memcpy(b,d,sizeof b);}
  if(reverse){for(unsigned i=0;i<2;i++){int32_t t=a[2+i];a[2+i]=a[4+i];a[4+i]=t;t=b[2+i];b[2+i]=b[4+i];b[4+i]=t;}}
  gif_init();gs_mem_init();g_gif.prim=3|PRIM_ABE_MASK;g_gif.prmodecont_ac=1;g_gif.fbp=0;g_gif.fbw=64;g_gif.zmsk=1;
  g_gif.alpha_a=0;g_gif.alpha_b=2;g_gif.alpha_c=2;g_gif.alpha_d=1;g_gif.alpha_fix=128;g_gif.colclamp=1;
  g_gif.ctx1_fbw=64;g_gif.ctx1_zmsk=1;
  g_gif.ctx1_alpha_a=0;g_gif.ctx1_alpha_b=2;g_gif.ctx1_alpha_c=2;g_gif.ctx1_alpha_d=1;g_gif.ctx1_alpha_fix=128;
  draw(a);draw(b);
  if(!diagonal&&!reverse)printf("Shared-edge RGB=%06x\n",gs_mem_read_psmct32(0,64,6,7)&0xffffff);
  gs_gx_flat_draw da,db;assert(gs_gx_prepare_flat(&da,3,0,64,2,3,10,11,a,0,0));assert(gs_gx_prepare_flat(&db,3,0,64,2,3,10,11,b,0,0));
  for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++){
   unsigned inside=x>=2&&x<10&&y>=3&&y<11;
   unsigned rgb=gs_mem_read_psmct32(0,64,x,y)&0xffffff;
   if(rgb!=(inside?0x181818u:0u)){printf("CPU duplicate/gap x=%u y=%u rgb=%x diagonal=%u reverse=%u\n",x,y,rgb,diagonal,reverse);return 1;}
   unsigned n=0;if(x>=2&&y>=3&&y<3+da.height){unsigned yy=y-3,xx=x-2;n+=(xx>=da.left[yy]&&xx<da.right[yy]);n+=(xx>=db.left[yy]&&xx<db.right[yy]);}
   assert(n==inside);
  }
 }
 puts("PASS shared diagonal blended once: both diagonals/windings, CPU pixels and GX row spans, no gaps or double coverage");return 0;
}
