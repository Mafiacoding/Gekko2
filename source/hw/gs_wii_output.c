/*
 * gs_wii_output.c - see include/core/hw/gs_wii_output.h.
 */

#include "core/hw/gs_wii_output.h"
#include "core/hw/gs_mem.h"

uint32_t gs_rgb8_pair_to_ycbcr(uint8_t r1, uint8_t g1, uint8_t b1,
                                uint8_t r2, uint8_t g2, uint8_t b2)
{
    /* Ported verbatim from libogc's console.c RGB8_to_YCbCr (BSD-style
     * license). The 16/240 clamping matches broadcast-safe YCbCr
     * range, not full 0-255 range - this is intentional and matches
     * what the Wii's video hardware/libogc itself does. */
    if (r1 < 16) r1 = 16;
    if (g1 < 16) g1 = 16;
    if (b1 < 16) b1 = 16;
    if (r2 < 16) r2 = 16;
    if (g2 < 16) g2 = 16;
    if (b2 < 16) b2 = 16;

    if (r1 > 240) r1 = 240;
    if (g1 > 240) g1 = 240;
    if (b1 > 240) b1 = 240;
    if (r2 > 240) r2 = 240;
    if (g2 > 240) g2 = 240;
    if (b2 > 240) b2 = 240;

    uint8_t Y1 = (uint8_t)((77 * r1 + 150 * g1 + 29 * b1) / 256);
    uint8_t Y2 = (uint8_t)((77 * r2 + 150 * g2 + 29 * b2) / 256);
    uint8_t Cb = (uint8_t)((112 * (b1 + b2) -  74 * (g1 + g2) - 38 * (r1 + r2)) / 512 + 128);
    uint8_t Cr = (uint8_t)((112 * (r1 + r2) - 94  * (g1 + g2) - 18 * (b1 + b2)) / 512 + 128);

    return ((uint32_t)Y1 << 24) | ((uint32_t)Cb << 16) | ((uint32_t)Y2 << 8) | (uint32_t)Cr;
}

static inline void rgba32_to_rgb8(uint32_t rgba, uint8_t *r, uint8_t *g, uint8_t *b)
{
    /* PSMCT32 channel order as stored by gs_mem_write_psmct32's caller
     * convention: 0xAABBGGRR (matches how we pack test/demo colors
     * elsewhere in this project - see tests). */
    *r = (uint8_t)(rgba & 0xFF);
    *g = (uint8_t)((rgba >> 8) & 0xFF);
    *b = (uint8_t)((rgba >> 16) & 0xFF);
}

/* Round 119 (task #172/#274) - see gs_wii_output.h's doc comment.
 * Moved verbatim from main.c's decode_dispfb() (task #126); logic is
 * byte-for-byte identical, only the name/location changed (renamed
 * with a gs_ prefix to match this file's own naming convention). */
void gs_decode_dispfb(uint64_t dispfb, uint32_t *out_bp_words, uint32_t *out_bw_pixels)
{
    uint32_t fbp_field = (uint32_t)(dispfb & 0x1FFu);
    uint32_t fbw_field  = (uint32_t)((dispfb >> 9) & 0x3Fu);
    *out_bp_words  = fbp_field * 2048u;
    *out_bw_pixels = fbw_field * 64u;
}

void gs_blit_psmct32_to_xfb(void *xfb, uint32_t xfb_width_px,
                             uint32_t dst_x, uint32_t dst_y,
                             uint32_t gs_bp, uint32_t gs_bw,
                             uint32_t src_x, uint32_t src_y,
                             uint32_t width, uint32_t height)
{
    uint32_t *xfb32 = (uint32_t *)xfb;
    uint32_t xfb_words_per_row = xfb_width_px / 2;

    for (uint32_t row = 0; row < height; row++) {
        for (uint32_t col = 0; col < width; col += 2) {
            uint32_t p1 = gs_mem_read_psmct32(gs_bp, gs_bw, src_x + col, src_y + row);
            uint32_t p2 = (col + 1 < width)
                ? gs_mem_read_psmct32(gs_bp, gs_bw, src_x + col + 1, src_y + row)
                : p1; /* odd trailing pixel: duplicate so the pair conversion still makes sense */

            uint8_t r1, g1, b1, r2, g2, b2;
            rgba32_to_rgb8(p1, &r1, &g1, &b1);
            rgba32_to_rgb8(p2, &r2, &g2, &b2);

            uint32_t yuv = gs_rgb8_pair_to_ycbcr(r1, g1, b1, r2, g2, b2);

            uint32_t dst_word_x = (dst_x + col) / 2;
            uint32_t dst_row = dst_y + row;
            xfb32[dst_row * xfb_words_per_row + dst_word_x] = yuv;
        }
    }
}

void gs_decode_display_region(uint64_t dispfb, uint64_t display, uint64_t smode2,
                              uint32_t *x, uint32_t *y, uint32_t *w, uint32_t *h)
{
    *x = (uint32_t)(dispfb >> 32) & 0x7ffu;
    *y = (uint32_t)(dispfb >> 48) & 0x7ffu;
    *w = (((uint32_t)(display >> 32) & 0xfffu) + 1u) /
         (((uint32_t)(display >> 23) & 15u) + 1u);
    *h = (((uint32_t)(display >> 44) & 0x7ffu) + 1u) /
         (((uint32_t)(display >> 27) & 3u) + 1u);
    /* PCSX2 GSState framebufferRect: INT+FFMD reads half the height. */
    if ((smode2 & 3u) == 3u) *h = (*h + 1u) / 2u;
}

/* Presentation filtering for the interlaced GS field. Cache converted
 * source lines, then interpolate Y/Cb/Cr vertically. GS textures and RAM
 * remain untouched; horizontal sampling retains the previous convention. */
static void gs_scaled_line(uint32_t *out,uint32_t dw,uint32_t bp,uint32_t bw,
                           uint32_t sx,uint32_t sy,uint32_t sw)
{
    /* Native-width scanout is the normal BIOS path. Hoist swizzle row
     * arithmetic and decode one contiguous line, retaining exact colors. */
    if(dw==sw && dw<=1024u) {
        uint32_t rgba[1024];
        gs_mem_read_psmct32_span(rgba,bp,bw,sx,sy,dw);
        for(uint32_t x=0;x<dw;x+=2u) {
            uint32_t a=rgba[x],b=rgba[x+1u];
            out[x/2u]=gs_rgb8_pair_to_ycbcr(a,a>>8,a>>16,b,b>>8,b>>16);
        }
        return;
    }
    uint32_t fx=0,step=(sw<<16)/dw;
    for(uint32_t x=0;x<dw;x+=2) {
        uint32_t a=gs_mem_read_psmct32(bp,bw,sx+(fx>>16),sy);fx+=step;
        uint32_t b=gs_mem_read_psmct32(bp,bw,sx+(fx>>16),sy);fx+=step;
        out[x/2]=gs_rgb8_pair_to_ycbcr(a,a>>8,a>>16,b,b>>8,b>>16);
    }
}
void gs_blit_scaled_psmct32_to_xfb(void *xfb,uint32_t dst_w,uint32_t dst_h,
                                  uint32_t bp,uint32_t bw,uint32_t sx,uint32_t sy,
                                  uint32_t sw,uint32_t sh)
{
    if(!xfb||!dst_w||!dst_h||!sw||!sh||(dst_w&1u))return;
    uint32_t *out=(uint32_t*)xfb;
    /* Bounded 4KB line cache; unusual wider outputs retain the old path. */
    if(dst_w>1024u) {
        uint32_t fy=0,step=(sh<<16)/dst_h;
        for(uint32_t y=0;y<dst_h;y++,fy+=step)
            gs_scaled_line(out+y*(dst_w/2),dst_w,bp,bw,sx,sy+(fy>>16),sw);
        return;
    }
    uint32_t rows[2][512],*a=rows[0],*b=rows[1];
    uint32_t ka=~0u,kb=~0u,step=(sh<<16)/dst_h;
    int64_t fy=((int64_t)step-65536)/2;
    for(uint32_t y=0;y<dst_h;y++,fy+=step) {
        uint32_t row=fy<0?0u:(uint32_t)(fy>>16);
        uint32_t f=fy<0?0u:(uint32_t)fy&65535u;
        if(row>=sh-1u){row=sh-1u;f=0;}
        uint32_t next=f?row+1u:row;
        if(kb==row){uint32_t *t=a;a=b;b=t;uint32_t k=ka;ka=kb;kb=k;}
        if(ka!=row){gs_scaled_line(a,dst_w,bp,bw,sx,sy+row,sw);ka=row;}
        if(f&&kb!=next){gs_scaled_line(b,dst_w,bp,bw,sx,sy+next,sw);kb=next;}
        for(uint32_t x=0;x<dst_w/2u;x++) {
            uint32_t v=a[x];
            if(f){v=0;for(unsigned c=0;c<32;c+=8)
                v|=(((((a[x]>>c)&255u)*(65536u-f)+((b[x]>>c)&255u)*f+32768u)>>16)&255u)<<c;}
            out[y*(dst_w/2u)+x]=v;
        }
    }
}

int gs_display_has_rgb(uint32_t bp,uint32_t bw,uint32_t sx,uint32_t sy,uint32_t sw,uint32_t sh)
{
    if(!bw||!sw||!sh)return 0;
    for(uint32_t y=0;y<16;y++)for(uint32_t x=0;x<32;x++)
        if(gs_mem_read_psmct32(bp,bw,sx+(uint64_t)x*sw/32,sy+(uint64_t)y*sh/16)&0xffffffu)return 1;
    return 0;
}
