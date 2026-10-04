#include "core/hw/gs_wii_output.h"
#include "core/hw/gs_mem.h"
void gs_blit_scaled_before_r1274(void *xfb, uint32_t dst_w, uint32_t dst_h,
                                  uint32_t bp, uint32_t bw,
                                  uint32_t sx, uint32_t sy, uint32_t sw, uint32_t sh)
{
    if (!xfb || !dst_w || !dst_h || !sw || !sh || (dst_w & 1u)) return;
    uint32_t *out = (uint32_t *)xfb;
    uint32_t step_x = (sw << 16) / dst_w;
    uint32_t step_y = (sh << 16) / dst_h;
    uint32_t fy = 0;
    for (uint32_t y = 0; y < dst_h; y++, fy += step_y) {
        uint32_t fx = 0;
        for (uint32_t x = 0; x < dst_w; x += 2) {
            uint32_t p1 = gs_mem_read_psmct32(bp, bw, sx + (fx >> 16), sy + (fy >> 16));
            fx += step_x;
            uint32_t p2 = gs_mem_read_psmct32(bp, bw, sx + (fx >> 16), sy + (fy >> 16));
            fx += step_x;
            uint8_t r1,g1,b1,r2,g2,b2;
            r1=p1;g1=p1>>8;b1=p1>>16;r2=p2;g2=p2>>8;b2=p2>>16;
            out[y * (dst_w / 2u) + x / 2u] = gs_rgb8_pair_to_ycbcr(r1,g1,b1,r2,g2,b2);
        }
    }
}


#include <stdio.h>
#include <stdlib.h>
#include "core/system.h"
#include "core/checkpoint.h"
#include "core/hw/gs.h"
static unsigned char clip(int x){return x<0?0:x>255?255:x;}
static void ppm(const char *path,const uint32_t *fb){FILE *f=fopen(path,"wb");fprintf(f,"P6\n640 480\n255\n");for(unsigned y=0;y<480;y++)for(unsigned x=0;x<640;x++){uint32_t p=fb[y*320+x/2];int yy=(p>>(x&1?8:24))&255,cb=((p>>16)&255)-128,cr=(p&255)-128;unsigned char rgb[3]={clip(yy+((359*cr)>>8)),clip(yy-((88*cb+183*cr)>>8)),clip(yy+((454*cb)>>8))};fwrite(rgb,1,3,f);}fclose(f);}
int main(int n,char **v){if(n!=5)return 2;bios_image_t b;if(bios_load(v[1],&b)||system_init(&b,&b)||checkpoint_load(v[2],&b,&b,0))return 3;gs_state_t*g=gs_get_state();uint64_t fb=(g->pmode&1)?g->dispfb1:g->dispfb2;uint32_t bp,bw,sx,sy,sw,sh;gs_decode_dispfb(fb,&bp,&bw);gs_decode_display_region(fb,(g->pmode&1)?g->display1:g->display2,g->smode2,&sx,&sy,&sw,&sh);printf("source=%ux%u output=640x480; identical GS checkpoint; presentation-only comparison\n",sw,sh);uint32_t *out=malloc(640*480*2);if(!out)return 4;gs_blit_scaled_before_r1274(out,640,480,bp,bw,sx,sy,sw,sh);ppm(v[3],out);gs_blit_scaled_psmct32_to_xfb(out,640,480,bp,bw,sx,sy,sw,sh);ppm(v[4],out);free(out);return 0;}
