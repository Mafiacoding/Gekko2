#include <stdio.h>
#include <stdint.h>
#include "core/hw/gs_gx.h"
#include "core/hw/gs_mem.h"
int main(void) {
  int fails=0,tests=0;
  const uint32_t formats[]={0,1,2,10};
  for(unsigned i=0;i<4;i++)for(unsigned j=0;j<4;j++){
    uint32_t p= formats[i];
    uint32_t max=p==1?0xffffffu:(p==2||p==10)?0xffffu:UINT32_MAX;
    uint32_t values[]={0,1,max-1,max};
    uint32_t v=values[j];
    tests++;if(gs_gx_depth_normalize(v,p)!=gs_mem_clamp_raster_z(v,p))fails++;
    tests++;if(gs_gx_depth_normalize(UINT32_MAX,p)!=max)fails++;
  }
  printf("BETA12.17 GX depth normalization: %d tests, %d failures\n",tests,fails);
  return !!fails;
}
