#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "core/hw/gs_mem.h"
#include "core/hw/gs_wii_output.h"
static unsigned checks,calls;
static int resolver_ok;
#define CHECK(e) do { checks++; if(!(e)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#e);return 1;} } while(0)
static int resolve(void *p,uint8_t *vram,uint32_t n)
{
    (void)p;(void)n;calls++;
    if(!resolver_ok)return 0;
    /* Swizzled CT32 pixels (0,0) and (1,0). */
    vram[0]=255;vram[1]=vram[2]=0;
    vram[4]=255;vram[5]=vram[6]=0;
    return 1;
}
int main(void)
{
    uint32_t out[4],old[4];
    for(unsigned i=0;i<4;i++)out[i]=old[i]=0x76543210u+i;
    gs_mem_init();
    CHECK(gs_mem_gpu_bind(resolve,NULL));
    CHECK(gs_mem_gpu_protect_range(0,8192));
    CHECK(gs_mem_gpu_mark_pending());
    CHECK(gs_blit_scaled_psm_to_xfb_checked(out+1,2,1,0,64,0,0,2,1,0)==-1);
    CHECK(memcmp(out,old,sizeof out)==0);
    CHECK(calls==1&&gs_mem_gpu_pending());
    /* Existing void API must also preserve the destination on failure. */
    gs_blit_scaled_psm_to_xfb(out+1,2,1,0,64,0,0,2,1,0);
    CHECK(memcmp(out,old,sizeof out)==0&&calls==2);
    CHECK(gs_blit_scaled_psm_to_xfb_checked(out+1,2,1,0,0,0,0,2,1,0)==0);
    CHECK(gs_blit_scaled_psm_to_xfb_checked(out+1,2,1,0,64,0,0,2,1,3)==0);
    CHECK(calls==2&&memcmp(out,old,sizeof out)==0);
    resolver_ok=1;
    CHECK(gs_blit_scaled_psm_to_xfb_checked(out+1,2,1,0,64,0,0,2,1,0)==1);
    CHECK(calls==3&&!gs_mem_gpu_pending());
    CHECK(out[1]==gs_rgb8_pair_to_ycbcr(255,0,0,255,0,0));
    CHECK(out[0]==old[0]&&out[2]==old[2]&&out[3]==old[3]);
    CHECK(gs_blit_scaled_psm_to_xfb_checked(out+1,2,1,0,64,0,0,2,1,0)==1);
    CHECK(calls==3); /* Already coherent: no repeated GPU resolve. */
    CHECK(gs_blit_scaled_psm_to_xfb_checked(out+1,2,1,0,64,UINT32_MAX,0,2,1,0)==0);
    printf("PASS scanout readback failure/retry: %u checks\n",checks);
    return 0;
}
