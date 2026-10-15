#include "core/hw/gs_gx_additive.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
 gs_gx_pipeline p={0};p.blend=1;p.a=0;p.b=2;p.c=2;p.d=1;p.colclamp=1;
 unsigned long checks=0;
 for(unsigned fix=0;fix<=128;fix++){
  p.fix=fix;assert(gs_gx_additive_supported(&p));
  for(unsigned src=0;src<256;src++){
   uint32_t rgba=0x81000000u|src|(src<<8)|(src<<16),scaled=gs_gx_additive_pixel(rgba,fix);
   assert((scaled>>24)==0x81);assert((scaled&255)==((src*fix)>>7));
   assert(((scaled>>8)&255)==(scaled&255));assert(((scaled>>16)&255)==(scaled&255));
   for(unsigned dst=0;dst<256;dst++){
    unsigned gx=(scaled&255)+dst,gs=((src*fix)>>7)+dst;
    if(gx>255)gx=255;if(gs>255)gs=255;assert(gx==gs);checks++;
   }
  }
 }
 p.fix=129;assert(!gs_gx_additive_supported(&p));p.fix=20;
 p.pabe=1;assert(!gs_gx_additive_supported(&p));p.pabe=0;
 p.colclamp=0;assert(!gs_gx_additive_supported(&p));p.colclamp=1;
 for(unsigned a=0;a<3;a++)for(unsigned b=0;b<3;b++)for(unsigned c=0;c<3;c++)for(unsigned d=0;d<3;d++){
  p.a=a;p.b=b;p.c=c;p.d=d;assert(gs_gx_additive_supported(&p)==(a==0&&b==2&&c==2&&d==1));
 }
 assert(!gs_gx_additive_supported(NULL));printf("PASS %lu exact additive channel combinations and eligibility/alpha gates\n",checks);return 0;
}
