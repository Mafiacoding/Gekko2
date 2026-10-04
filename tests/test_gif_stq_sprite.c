/* test_gif_stq_sprite.c - host-native test for task #88: perspective-
 * correct (ST+Q, PRIM's FST=0 mode) texture coordinates on triangles,
 * and texturing support for the SPRITE rasterizer. See
 * include/core/hw/gif.h's scope comment and gif.c's rasterize_sprite()
 * for exactly what's modeled and the SPRITE approximation's rationale.
 */
#include <stdio.h>
#include <string.h>
#include "hw/gs_mem.c"
#include "hw/gif.c"

/* Round 640: seed texture/CLUT data via the _blk (real 256-bytes/unit
 * BITBLTBUF/TEX0-style) addressing helper, matching gif.c's gs_sample_
 * texel()/gs_sample_clut() which now read TBP0/CBP through the same
 * _blk scale. Framebuffer output reads below stay on the plain,
 * unchanged gs_mem_read_psmct32() - FBP/output addressing is untouched
 * by this round's fix. See docs/STATUS.md Round 639/640. */

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("ok:   %s\n", msg); } \
} while (0)

static void wle32(uint8_t *p, uint32_t v) { p[0]=v&0xFF;p[1]=(v>>8)&0xFF;p[2]=(v>>16)&0xFF;p[3]=(v>>24)&0xFF; }

static void append_ad(uint8_t *buf, int *off, uint32_t data_lo, uint32_t data_hi, uint32_t addr)
{
    if (addr == GS_REG_FRAME_1 || addr == GS_REG_FRAME_2) data_lo = (data_lo & 0x1ffu) | (((data_lo >> 9) & 0x3fu) << 16);
    /* Encode fixture pixel coordinates into the real 64-bit XYZ register. */
    if (addr == GS_REG_XYZ2 || addr == GS_REG_XYZ3 || addr == GS_REG_XYZF2 || addr == GS_REG_XYZF3) {
        data_lo = (data_lo & 0xffffu) | ((data_hi & 0xffffu) << 16); data_hi = 0u;
    }
    wle32(buf + *off, data_lo);
    wle32(buf + *off + 4, data_hi);
    wle32(buf + *off + 8, addr);
    wle32(buf + *off + 12, 0);
    *off += 16;
}

static uint8_t chan(uint32_t rgba, int shift) { return (uint8_t)((rgba >> shift) & 0xFFu); }

static uint32_t float_bits(float f)
{
    uint32_t v;
    memcpy(&v, &f, sizeof(v));
    return v;
}

static void fill_texture_gradient_red(uint32_t bp, uint32_t bw, uint32_t w)
{
    for (uint32_t x = 0; x < w; x++)
        gs_mem_write_psmct32_blk(bp, bw, x, 0, ((uint32_t)0xFFu << 24) | (x * 20u)); /* r=x*20, rest 0, a=255 */
}

int main(void)
{
    /* At centroid (3,3), weights are 1/3. GS interpolates S,T,Q
     * before division: S=3, Q=2, so S/Q=1.5. The existing nearest
     * sampler rounds this to texel 2. Reciprocal-Q interpolation
     * incorrectly produces texel 4. */
    gs_mem_init();
    gif_init();
    {
        uint32_t tex_bp = 3000, tex_bw = 64;
        fill_texture_gradient_red(tex_bp, tex_bw, 10);

        uint8_t buf[16 * (1 + 4 + 3 * 3)]; /* tag + FRAME_1/XYOFFSET_1/TEX0_1/PRIM + 3*(RGBAQ+ST+XYZ2) */
        memset(buf, 0, sizeof(buf));
        int off = 0;
        int nloop = 4 + 3 * 3;
        wle32(buf + off, (uint32_t)nloop | (1u << 15));
        wle32(buf + off + 4, (0u << 26) | (1u << 28));
        wle32(buf + off + 8, GIF_REG_AD);
        wle32(buf + off + 12, 0);
        off += 16;

        append_ad(buf, &off, (10u << 9), 0, GS_REG_FRAME_1);
        append_ad(buf, &off, 0, 0, GS_REG_XYOFFSET_1);
        /* TW=0, TH=0: word0 bits 26-29 (TW) = 0, bits 30-31 = 0; word1 bits 0-1 (TH high bits) = 0. */
        append_ad(buf, &off, (tex_bp & 0x3FFFu) | (((tex_bw / 64u) & 0x3Fu) << 14), (TEX_TFX_DECAL << 3), GS_REG_TEX0_1);
        /* No PRIM_FST_MASK: FST=0, ST+Q mode. */
        append_ad(buf, &off, (uint32_t)PRIM_TYPE_TRIANGLE | PRIM_TME_MASK, 0, GS_REG_PRIM);

        uint32_t dummy_color = 0xFF7F7F7Fu; /* irrelevant under DECAL */
        int32_t verts[3][2] = { { 0, 0 }, { 9, 0 }, { 0, 9 } };
        float ss[3] = { 0.0f, 9.0f, 0.0f };
        float qq[3] = { 1.0f, 1.0f, 4.0f };
        for (int i = 0; i < 3; i++) {
            /* RGBAQ: R/G/B/A in word0, Q as a real float in word1. */
            append_ad(buf, &off, dummy_color, float_bits(qq[i]), GS_REG_RGBAQ);
            /* ST: S in word0, T in word1 (both real floats; T=0 for all - unused this test). */
            append_ad(buf, &off, float_bits(ss[i]), float_bits(0.0f), GS_REG_ST);
            append_ad(buf, &off, (uint32_t)(verts[i][0] << 4), (uint32_t)(verts[i][1] << 4), GS_REG_XYZ2);
        }

        gif_process_quadwords(DMA_CHANNEL_GIF, buf, (uint32_t)(off / 16));

        uint32_t px = gs_mem_read_psmct32(0, 640, 3, 3); /* the exact centroid */
        CHECK(chan(px, 0) == 2u * 20u,
              "STQ varying Q: interpolated S=3 divided by Q=2 samples texel 2");
    }

    /* Constant Q=2 must still divide coordinates: centroid S=3,
     * S/Q=1.5, nearest texel 2. This catches accidental Q cancellation. */
    gs_mem_init();
    gif_init();
    {
        uint32_t tex_bp = 3100, tex_bw = 64;
        fill_texture_gradient_red(tex_bp, tex_bw, 10);

        uint8_t buf[16 * (1 + 4 + 3 * 3)];
        memset(buf, 0, sizeof(buf));
        int off = 0;
        int nloop = 4 + 3 * 3;
        wle32(buf + off, (uint32_t)nloop | (1u << 15));
        wle32(buf + off + 4, (0u << 26) | (1u << 28));
        wle32(buf + off + 8, GIF_REG_AD);
        wle32(buf + off + 12, 0);
        off += 16;

        append_ad(buf, &off, (10u << 9), 0, GS_REG_FRAME_1);
        append_ad(buf, &off, 0, 0, GS_REG_XYOFFSET_1);
        append_ad(buf, &off, (tex_bp & 0x3FFFu) | (((tex_bw / 64u) & 0x3Fu) << 14), (TEX_TFX_DECAL << 3), GS_REG_TEX0_1);
        append_ad(buf, &off, (uint32_t)PRIM_TYPE_TRIANGLE | PRIM_TME_MASK, 0, GS_REG_PRIM); /* FST=0 */

        uint32_t dummy_color = 0xFF7F7F7Fu;
        int32_t verts[3][2] = { { 0, 0 }, { 9, 0 }, { 0, 9 } };
        float ss[3] = { 0.0f, 9.0f, 0.0f };
        for (int i = 0; i < 3; i++) {
            append_ad(buf, &off, dummy_color, float_bits(2.0f), GS_REG_RGBAQ); /* Q=2.0 everywhere */
            append_ad(buf, &off, float_bits(ss[i]), float_bits(0.0f), GS_REG_ST);
            append_ad(buf, &off, (uint32_t)(verts[i][0] << 4), (uint32_t)(verts[i][1] << 4), GS_REG_XYZ2);
        }

        gif_process_quadwords(DMA_CHANNEL_GIF, buf, (uint32_t)(off / 16));

        uint32_t px = gs_mem_read_psmct32(0, 640, 3, 3);
        CHECK(chan(px, 0) == 2u * 20u,
              "STQ constant Q=2: centroid S=3 remains divided by Q");
    }

    /* --- SPRITE texturing, FST=1 (UV): axis-aligned bilinear
     * interpolation between the 2 corners' UV values. Identity
     * mapping (corner0 u=0,v=0 at (0,0); corner1 u=10,v=10 at
     * (10,10)) over a texture where texel(x,y) red=x*10, green=y*10 -
     * midpoint (5,5) must sample texel (5,5) exactly. --- */
    gs_mem_init();
    gif_init();
    {
        uint32_t tex_bp = 3200, tex_bw = 64;
        for (uint32_t y = 0; y < 11; y++)
            for (uint32_t x = 0; x < 11; x++)
                gs_mem_write_psmct32_blk(tex_bp, tex_bw, x, y, ((uint32_t)0xFFu << 24) | (y * 10u << 8) | (x * 10u));

        uint8_t buf[16 * (1 + 4 + 2 * 3)]; /* tag + FRAME_1/XYOFFSET_1/TEX0_1/PRIM + 2*(RGBAQ+UV+XYZ2) */
        memset(buf, 0, sizeof(buf));
        int off = 0;
        int nloop = 4 + 2 * 3;
        wle32(buf + off, (uint32_t)nloop | (1u << 15));
        wle32(buf + off + 4, (0u << 26) | (1u << 28));
        wle32(buf + off + 8, GIF_REG_AD);
        wle32(buf + off + 12, 0);
        off += 16;

        append_ad(buf, &off, (10u << 9), 0, GS_REG_FRAME_1);
        append_ad(buf, &off, 0, 0, GS_REG_XYOFFSET_1);
        append_ad(buf, &off, (tex_bp & 0x3FFFu) | (((tex_bw / 64u) & 0x3Fu) << 14), (TEX_TFX_DECAL << 3), GS_REG_TEX0_1);
        append_ad(buf, &off, (uint32_t)PRIM_TYPE_SPRITE | PRIM_TME_MASK | PRIM_FST_MASK, 0, GS_REG_PRIM);

        uint32_t dummy_color = 0xFF7F7F7Fu;
        /* GS_REG_UV real packing (GIFRegUV: u16 U; u16 V; u32 _PAD -
         * BOTH U and V live in word0/data_lo, U in the low 16 bits, V
         * in the high 16 bits; word1/data_hi is unused padding). */
        append_ad(buf, &off, dummy_color, 0, GS_REG_RGBAQ);
        append_ad(buf, &off, (0u << 4) | ((0u << 4) << 16), 0, GS_REG_UV);
        append_ad(buf, &off, (0u << 4), (0u << 4), GS_REG_XYZ2);
        append_ad(buf, &off, dummy_color, 0, GS_REG_RGBAQ);
        append_ad(buf, &off, (10u << 4) | ((10u << 4) << 16), 0, GS_REG_UV);
        append_ad(buf, &off, (10u << 4), (10u << 4), GS_REG_XYZ2);

        gif_process_quadwords(DMA_CHANNEL_GIF, buf, (uint32_t)(off / 16));

        gif_state_t *gs = gif_get_state();
        uint32_t px = gs_mem_read_psmct32(0, 640, 5, 5);
        CHECK(gs->sprites_drawn == 1, "SPRITE texturing: exactly one sprite drawn");
        CHECK(chan(px, 0) == 50 && chan(px, 8) == 50,
              "SPRITE texturing (FST=1/UV): midpoint (5,5) samples texel (5,5) via axis-aligned bilinear interpolation");
    }

    /* --- SPRITE with TME=0: still flat-colored, unaffected by the
     * new texturing code path (regression check). --- */
    gs_mem_init();
    gif_init();
    {
        uint8_t buf[16 * (1 + 3 + 2 * 2)]; /* tag + FRAME_1/XYOFFSET_1/PRIM + 2*(RGBAQ+XYZ2) */
        memset(buf, 0, sizeof(buf));
        int off = 0;
        int nloop = 3 + 2 * 2;
        wle32(buf + off, (uint32_t)nloop | (1u << 15));
        wle32(buf + off + 4, (0u << 26) | (1u << 28));
        wle32(buf + off + 8, GIF_REG_AD);
        wle32(buf + off + 12, 0);
        off += 16;

        append_ad(buf, &off, (10u << 9), 0, GS_REG_FRAME_1);
        append_ad(buf, &off, 0, 0, GS_REG_XYOFFSET_1);
        append_ad(buf, &off, (uint32_t)PRIM_TYPE_SPRITE, 0, GS_REG_PRIM); /* no TME */

        uint32_t yellow = 0xFF00FFFFu;
        append_ad(buf, &off, yellow, 0, GS_REG_RGBAQ);
        append_ad(buf, &off, (0u << 4), (0u << 4), GS_REG_XYZ2);
        append_ad(buf, &off, yellow, 0, GS_REG_RGBAQ);
        append_ad(buf, &off, (10u << 4), (10u << 4), GS_REG_XYZ2);

        gif_process_quadwords(DMA_CHANNEL_GIF, buf, (uint32_t)(off / 16));

        uint32_t px = gs_mem_read_psmct32(0, 640, 5, 5);
        CHECK(px == yellow, "SPRITE TME=0 regression: still flat-colored, unaffected by the new texturing code path");
    }

    printf("\n%d check(s) failed\n", failures);
    return failures ? 1 : 0;
}
