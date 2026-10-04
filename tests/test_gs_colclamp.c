/* Texture arithmetic saturates before framebuffer blending. COLCLAMP
 * controls framebuffer RGB overflow, independently of texture alpha. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "hw/gif.c"
#include "core/hw/gs_mem.h"

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

static void wle32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static void write_tag(uint8_t *buf, int *off, uint32_t nloop, uint32_t regs_nibble0)
{
    uint32_t w0 = nloop & 0x7FFFu;
    uint32_t w1 = (0u << 26) | (1u << 28);
    uint32_t w2 = regs_nibble0 & 0xFu;
    uint32_t w3 = 0u;
    wle32(buf + *off, w0); wle32(buf + *off + 4, w1);
    wle32(buf + *off + 8, w2); wle32(buf + *off + 12, w3);
    *off += 16;
}

static void append_ad(uint8_t *buf, int *off, uint32_t data_lo, uint32_t data_hi, uint32_t addr)
{
    if (addr == GS_REG_FRAME_1 || addr == GS_REG_FRAME_2) data_lo = (data_lo & 0x1ffu) | (((data_lo >> 9) & 0x3fu) << 16);
    /* Encode fixture pixel coordinates into the real 64-bit XYZ register. */
    if (addr == GS_REG_XYZ2 || addr == GS_REG_XYZ3 || addr == GS_REG_XYZF2 || addr == GS_REG_XYZF3) {
        data_lo = (data_lo & 0xffffu) | ((data_hi & 0xffffu) << 16); data_hi = 0u;
    }
    wle32(buf + *off, data_lo); wle32(buf + *off + 4, data_hi);
    wle32(buf + *off + 8, addr); wle32(buf + *off + 12, 0);
    *off += 16;
}

#define TEX_BP 3000u
#define TEX_BW 64u

static void setup_white_texel(void)
{
    gs_mem_write_psmct32_blk(TEX_BP, TEX_BW, 0, 0, 0xFFFFFFFFu);
}

/* Draws a 1x1 MODULATE-textured SPRITE with white texel * white vertex
 * color (both 255,255,255) - overflows to 508 per channel before any
 * clamp/mask is applied. Optionally writes COLCLAMP first. */
static uint32_t sample_modulate(int write_colclamp, uint32_t clamp_bit)
{
    gs_mem_init();
    gif_init();
    setup_white_texel();

    uint8_t buf[16 * 10];
    memset(buf, 0, sizeof(buf));
    int off = 0;
    uint32_t nloop = 4 + (write_colclamp ? 1 : 0) + 3;
    write_tag(buf, &off, nloop, 0xE);
    append_ad(buf, &off, (10u << 9), 0, GS_REG_FRAME_1);
    append_ad(buf, &off, 0, 0, GS_REG_XYOFFSET_1);
    uint32_t tex0_lo = (TEX_BP & 0x3FFFu) | (((TEX_BW / 64u) & 0x3Fu) << 14) | (0u << 26); /* TW=0 (1 texel) */
    uint32_t tex0_hi = ((1u << 2) | (TEX_TFX_MODULATE << 3));
    append_ad(buf, &off, tex0_lo, tex0_hi, GS_REG_TEX0_1);
    if (write_colclamp)
        append_ad(buf, &off, clamp_bit & 0x1u, 0u, GS_REG_COLCLAMP);
    append_ad(buf, &off, (uint32_t)PRIM_TYPE_SPRITE | PRIM_TME_MASK | PRIM_FST_MASK, 0, GS_REG_PRIM);
    append_ad(buf, &off, 0xFFFFFFFFu, 0, GS_REG_RGBAQ); /* white vertex color */
    append_ad(buf, &off, (uint32_t)(0 << 4), (uint32_t)(0 << 4), GS_REG_UV);
    append_ad(buf, &off, (uint32_t)(0 << 4), (uint32_t)(0 << 4), GS_REG_XYZ2);

    uint8_t buf2[16 * 2];
    int off2 = 0;
    write_tag(buf2, &off2, 1, 0xE);
    append_ad(buf2, &off2, (uint32_t)(1 << 4), (uint32_t)(1 << 4), GS_REG_XYZ2);

    gif_process_quadwords(DMA_CHANNEL_GIF, buf, (uint32_t)(off / 16));
    gif_process_quadwords(DMA_CHANNEL_GIF, buf2, (uint32_t)(off2 / 16));

    return gs_mem_read_psmct32(0, 640, 0, 0);
}

int main(void)
{
    { /* No COLCLAMP write at all (default, safety gate): the overflow
       * (255*255/128=508) clamps to 255 exactly as before this round -
       * a genuine no-op regression check for every pre-existing test/
       * demo that never touches COLCLAMP. */
        uint32_t px = sample_modulate(0, 0);
        CHECK(px == 0xFFFFFFFFu,
              "no COLCLAMP write: modulate overflow (508) clamps to 255 (regression safety)");
    }

    { /* COLCLAMP.CLAMP=1 written explicitly: same clamped behavior. */
        uint32_t px = sample_modulate(1, 1);
        CHECK(px == 0xFFFFFFFFu,
              "COLCLAMP CLAMP=1: modulate overflow clamps to 255");
    }

    { /* Texture-stage saturation is independent of COLCLAMP. */
        CHECK(sample_modulate(1, 0) == 0xFFFFFFFFu,
              "COLCLAMP MASK retains saturated texture result");
        g_gif.prim |= PRIM_ABE_MASK;
        g_gif.alpha_a = GS_ALPHA_CS; g_gif.alpha_b = GS_ALPHA_ZERO;
        g_gif.alpha_c = GS_ALPHA_AS; g_gif.alpha_d = GS_ALPHA_ZERO;
        gs_finish_pixel(0, 0, 0xFFFFFFFFu, 0u, 0);
        CHECK(gs_mem_read_psmct32(0, 640, 0, 0) == 0xFFFCFCFCu,
              "COLCLAMP MASK wraps framebuffer blend RGB 508 to 252, retains alpha");
        g_gif.colclamp = 1;
        gs_finish_pixel(0, 0, 0xFFFFFFFFu, 0u, 0);
        CHECK(gs_mem_read_psmct32(0, 640, 0, 0) == 0xFFFFFFFFu,
              "COLCLAMP CLAMP saturates framebuffer blend RGB");
    }

    printf(failures == 0 ? "ALL TESTS PASSED\n" : "SOME TESTS FAILED\n");
    return failures == 0 ? 0 : 1;
}
