/* Exact GS ((Cs-0)*FIX >> 7)+Cd with final saturation. RGB source
 * scaling is part of CPU shading; GX performs ONE+ONE destination blending.
 * Source alpha is unchanged for GS metadata. No destination copy is needed. */
#include "core/hw/gs_gx_additive.h"
int gs_gx_additive_supported(const gs_gx_pipeline *p)
{return p&&p->blend&&p->a==0&&p->b==2&&p->c==2&&p->d==1&&p->fix<=128&&p->colclamp&&!p->pabe;}
uint32_t gs_gx_additive_pixel(uint32_t rgba,uint32_t fix)
{
 return (rgba&0xff000000u)|(((rgba&255u)*fix)>>7)|
  (((((rgba>>8)&255u)*fix)>>7)<<8)|(((((rgba>>16)&255u)*fix)>>7)<<16);
}
