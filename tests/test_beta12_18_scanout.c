#include <stdint.h>
#include <stdio.h>
#include "core/hw/gs_mem.h"
#include "core/hw/gs_wii_output.h"
static int check(int cond,const char *label){if(!cond)fprintf(stderr,"FAIL %s\n",label);return cond;}
int main(void)
{
    uint32_t stats[6];
    int good=1;
    gs_mem_init();
    /* CT16 RGB555 - red, green - alpha MSB is not a scanout color bit. */
    gs_mem_write_psmct16(0,64,12,6,0x801fu);
    gs_mem_write_psmct16(0,64,13,6,0x03e0u);
    good&=check(gs_scanout_read_rgb(0,64,12,6,2)==0x000000ffu,"CT16 red expansion");
    good&=check(gs_scanout_read_rgb(0,64,13,6,2)==0x0000ff00u,"CT16 green expansion");
    good&=check(gs_display_has_rgb_psm(0,64,12,6,2,1,2),"CT16 first-image probe");
    uint32_t packed[1]={0};
    gs_blit_scaled_psm_to_xfb(packed,2,1,0,64,12,6,2,1,2);
    good&=check(packed[0]==gs_rgb8_pair_to_ycbcr(255,0,0,0,255,0),"CT16 Wii YUV output");
    gs_scanout_probe_rgb_psm(0,64,12,6,2,1,2,stats);
    good&=check(stats[0]==64 && stats[5]==64,"CT16 nonblack diagnostic");
    /* CT16S is not CT16's page/block permutation. */
    gs_mem_write_psmct16s(2048,64,24,39,0x7c00u);
    good&=check(gs_scanout_read_rgb(2048,64,24,39,10)==0x00ff0000u,"CT16S blue expansion");
    good&=check(gs_display_has_rgb_psm(2048,64,24,39,1,1,10),"CT16S image probe");
    gs_mem_write_psmct32(4096,64,4,5,0xab010203u);
    good&=check(gs_scanout_read_rgb(4096,64,4,5,0)==0x00010203u,"CT32 compatibility");
    good&=check(gs_scanout_read_rgb(4096,64,4,5,1)==0x00010203u,"CT24 physical word compatibility");
    good&=check(gs_scanout_read_rgb(4096,64,4,5,3)==0,"unsupported PSM stays black");
    packed[0]=0x12345678u;
    gs_blit_scaled_psm_to_xfb(packed,2,1,4096,64,4,5,2,1,3);
    good&=check(packed[0]==0x12345678u,"unsupported format never writes XFB");
    if(good)puts("PASS BETA12.18 CT16/CT16S/CT24/CT32 scanout, FIRST_IMAGE, XFB, guards");
    return good?0:1;
}
