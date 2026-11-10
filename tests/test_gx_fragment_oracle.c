/* Compare new native permissions to the existing software rasterizer. */
#include <assert.h>
#include <stdio.h>
#include "core/hw/gs_gx_surface.h"
#include "hw/gif.c"
int main(void){
 unsigned count=0;const unsigned alphas[]={0,64,127,128,255};
 gif_init();gs_mem_init();gekko2_optimization_mask=0;
 apply_ad_write(GS_REG_PRIM,PRIM_TYPE_TRIANGLE|PRIM_ABE_MASK,0);
 apply_ad_write(GS_REG_ALPHA_1,0u|(1u<<2)|(0u<<4)|(1u<<6),0);
 for(unsigned fmt=0;fmt<2;fmt++)for(unsigned at=0;at<8;at++)for(unsigned af=0;af<4;af++)
 for(unsigned date=0;date<2;date++)for(unsigned datm=0;datm<2;datm++)for(unsigned zt=0;zt<4;zt++)
 for(unsigned ai=0;ai<5;ai++)for(unsigned fba=0;fba<2;fba++)for(unsigned pabe=0;pabe<2;pabe++)for(unsigned zi=0;zi<4;zi++){
  unsigned zpsm=zi==3?10:zi,sa=alphas[ai],da=(at&1)?128:0,z=4+(ai&1),oldz=4+(at&2);
  gs_gx_pipeline p={.ate=1,.atst=at,.aref=128,.afail=af,.date=date,.datm=datm,.ztest=1,.ztst=zt,.zwrite=1,.blend=1,.pabe=pabe,.zpsm=zpsm};
  unsigned actions=gs_gx_fragment_actions(&p,fmt,da,sa,z,oldz),old=0x00201008|(da<<24),src=0x00604020|(sa<<24);
  apply_ad_write(GS_REG_FRAME_1,(1u<<16)|(fmt<<24),0);
  apply_ad_write(GS_REG_ZBUF_1,2u|(zpsm<<24),0);
  apply_ad_write(GS_REG_TEST_1,1u|(at<<1)|(128u<<4)|(af<<12)|(date<<14)|(datm<<15)|(1u<<16)|(zt<<17),0);
  apply_ad_write(GS_REG_FBA_1,fba,0);apply_ad_write(GS_REG_PABE,pabe,0);
  gs_mem_write_psmct32(0,64,1,1,old);gs_mem_write_z(4096,64,1,1,zpsm,oldz);
  rasterize_triangle(0,0,4,0,0,4,src,src,src,0,0,0,0,0,0,0,0,1,0,0,1,0,0,1,z,z,z,0,0,0);
  unsigned expected=old;
  if(actions&GS_GX_WRITE_COLOR){expected=src&0xffffff;
   if(actions&GS_GX_DO_BLEND){expected=0;for(unsigned c=0;c<3;c++){unsigned s=(src>>(c*8))&255,d=(old>>(c*8))&255;expected|=(((s-d)*sa/128)+d)<<(c*8);}}
   expected|=((actions&GS_GX_WRITE_ALPHA)&&!fmt?(sa|(fba?128:0)):da)<<24;
  }
  assert(gs_mem_read_psmct32(0,64,1,1)==expected);
  assert(gs_mem_read_z(4096,64,1,1,zpsm)==((actions&GS_GX_WRITE_Z)?z:oldz));count++;
 }
 printf("PASS %u native permission cases against software GS rasterization: CT32/24, all ATE/AFAIL/DATE/Z, four depth formats, FBA/PABE\n",count);
}
