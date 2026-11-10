#include <assert.h>
#include <stdio.h>
#include "hw/gif.c"
int main(void)
{
 const unsigned formats[]={0,1,2,10};
 for(unsigned f=0;f<4;f++)for(unsigned dthe=0;dthe<2;dthe++){
  gif_init();gs_mem_init();
  apply_ad_write(GS_REG_FRAME_1,(1u<<16)|(formats[f]<<24),0);
  apply_ad_write(GS_REG_DTHE,dthe,0);
  apply_ad_write(GS_REG_PRIM,PRIM_TYPE_SPRITE,0);gs_activate_context();
  assert(gs_dither_active()==(int)(dthe&&f>=2));
  assert(g_gx_opaque==(f<2));
  assert(g_opaque_pixel==(f==0));
  assert(g_gif.dthe==dthe); /* normalization does not alter the GS register */
 }
 puts("PASS GS dither admission: CT24/32 retain opaque/GX eligibility, CT16/16S keep dither, raw register unchanged");
 return 0;
}
