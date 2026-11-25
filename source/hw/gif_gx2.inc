/* GPL-3.0-or-later. Wii U only (GEKKO2_WIIU_GX2): GS triangles and sprites rasterised by the Wii U GPU.
 * Included by gif.c. The software rasteriser stays the reference and takes every draw this path rejects.
 *
 * Ownership uses gs_mem's deferred GPU resolver. The first GPU draw into a FRAME uploads that buffer (fbw x
 * height, PSMCT32/24) into the GPU colour target; a draw that tests or writes Z also uploads the Z buffer
 * (Z32/24/16/16S) into the GPU depth buffer. GS memory is then marked pending and both physical ranges are
 * protected. Any CPU access overlapping them (display read, texture from the frame/depth buffer, transfers,
 * software draws) resolves first: GPU work completes and changed pixels/depths are written back into GS memory.
 * Pixels the GPU did not change are not rewritten, so frame buffer alpha above 0x80 and Z32 low bits survive.
 * A resolved target is closed; the next GPU draw uploads again.
 *
 * Accepted state (everything else -> software): FRAME PSMCT32/24 without FBMSK/FBA, no DATE, no dither, no fog,
 * no SCANMSK, depth test NEVER/ALWAYS/GEQUAL/GREATER (needs the GPU depth buffer), alpha test off or ALWAYS,
 * COLCLAMP on, no PABE, blend equations representable by GX2 (g2_blend), TFX MODULATE or DECAL, no mipmapping,
 * wrap REPEAT/CLAMP, up to 1024x1024 texels, Q > 0.
 * Known differences: GPU-drawn pixels saturate frame buffer alpha at 0x80; colours are interpolated perspective-
 * correctly for STQ textures; triangle edges follow the GPU fill rule; depth is 24-bit (Z32 compares on the top
 * 24 bits). */
#include "gx2_gs.h"
#ifdef G2_HOST_PROF
#include <time.h>
static uint64_t g2_ns[4]; /* 0 upload, 1 resolve, 2 texture decode, 3 gpu draw calls */
static uint64_t g2_now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec;}
uint64_t gekko2_gx2_gs_ns(unsigned i){return i<4?g2_ns[i]:0;}
#define G2_T0 uint64_t g2_t0 = g2_now()
#define G2_T1(i) (g2_ns[(i)] += g2_now() - g2_t0)
#else
#define G2_T0 ((void)0)
#define G2_T1(i) ((void)0)
#endif

int gekko2_gx2_gs_enabled;
static struct {
    int open, zopen, zdirty;
    uint32_t fbp, fbw, psm, w, h, lo, hi;
    uint32_t zbp, zpsm, zlo, zhi;
    /* Row bands: uploaded to the GPU (u) and drawn by the GPU (d), colour and depth. Empty when y0 >= y1. */
    uint32_t uy0, uy1, dy0, dy1, zuy0, zuy1, zdy0, zdy1;
} g2r;
static struct { int valid; uint32_t lo, hi, w, h, linear, wrap, rx0, ry0, rx1, ry1, epoch; uint64_t data; } g2t;
static uint64_t g2_stats[12];
static uint64_t g2_cause[4]; /* resolves caused by: 0 texture overlap, 1 target switch, 2 other CPU access */
uint64_t gekko2_gx2_gs_cause(unsigned i) { return i < 4 ? g2_cause[i] : 0; }
static int g2_cause_now = 2;
/* 0 GPU triangles, 1 GPU sprites, 2 target uploads, 3 resolves, 4 rejects, 5 texture uploads,
 * 6 texture reuses, 7 resolver failures, 8 upload pixels, 9 readback pixels, 10 depth uploads, 11 depth resolves */
uint64_t gekko2_gx2_gs_stat(unsigned i) { return i < 12 ? g2_stats[i] : 0; }
/* Reject reasons: 0 frame format, 1 fbmask/fba/date/dither/fog/scanmsk, 2 depth, 3 alpha test, 4 colclamp,
 * 5 pabe, 6 blend equation, 7 texture function, 8 mipmap, 9 texture size, 10 clamp mode, 11 Q <= 0,
 * 12 texture bind, 13 target open */
static uint64_t g2_reasons[16];
uint64_t gekko2_gx2_gs_reason(unsigned i) { return i < 16 ? g2_reasons[i] : 0; }
#define G2_REJECT(n) do { g2_reasons[(n)]++; return -1; } while (0)

static uint32_t g2_alpha_to_gs(uint32_t a8) { return (a8 * 128u + 127u) / 255u; }
/* Colour word the upload produced for these GS bytes (0xRRGGBBAA, alpha 0x80 -> 0xff). */
static uint32_t g2_upload_word(const uint8_t *s, uint32_t psm)
{
    uint32_t a = psm == TEX_PSM_PSMCT24 ? 0xffu : (uint32_t)s[3] * 2u;
    if (a > 0xffu) a = 0xffu;
    return ((uint32_t)s[0] << 24) | ((uint32_t)s[1] << 16) | ((uint32_t)s[2] << 8) | a;
}
/* GS Z <-> normalised depth. 24-bit UNORM depth d = round(n * 0xffffff). */
static double g2_zscale(uint32_t zpsm) { return zpsm == 0u ? 4294967295.0 : zpsm == 1u ? 16777215.0 : 65535.0; }
static uint32_t g2_z_to_d24(uint32_t z, uint32_t zpsm) { return (uint32_t)((double)z / g2_zscale(zpsm) * 16777215.0 + 0.5); }
static uint32_t g2_d24_to_z(uint32_t d, uint32_t zpsm)
{
    double z = (double)d / 16777215.0 * g2_zscale(zpsm) + 0.5;
    return z >= g2_zscale(zpsm) ? (uint32_t)g2_zscale(zpsm) : (uint32_t)z;
}
static uint32_t g2_read_zbytes(const uint8_t *p, uint32_t zpsm)
{
    const unsigned n = zpsm == 0u ? 4u : zpsm == 1u ? 3u : 2u;
    uint32_t v = 0;
    for (unsigned i = 0; i < n; i++) v |= (uint32_t)p[i] << (8u * i);
    return v;
}

#ifdef G2_HOST_PROF
static uint32_t g2_shadow[1024u * 1024u];
#endif
static int g2_resolver_body(void *opaque, uint8_t *vram, uint32_t size);
/* GX2 may only be used from the core that initialised it. With the GPU renderer on its own core the GS worker runs
 * there, but a resolve can also be requested by the emulation thread (picture output, detach); the front end then
 * runs the body on the GX2 thread and waits. NULL hook: call directly. */
int (*gekko2_gx2_call)(int (*fn)(void *, uint8_t *, uint32_t), void *opaque, uint8_t *vram, uint32_t size);
static int g2_resolver(void *opaque, uint8_t *vram, uint32_t size)
{
    if (!g2r.open)
        return 1;
    if (gekko2_gx2_call)
        return gekko2_gx2_call(g2_resolver_body, opaque, vram, size);
    return g2_resolver_body(opaque, vram, size);
}
static int g2_resolver_body(void *opaque, uint8_t *vram, uint32_t size)
{
    (void)opaque;
    if (!g2r.open)
        return 1;
    G2_T0;
    unsigned pitch = 0;
    const uint32_t *px = gx2_gs_end_rows(g2r.dy0, g2r.dy1, &pitch);
    if (!px) {
        g2_stats[7]++;
        return 0;
    }
    const uint32_t page = g2r.fbp / 2048u;
    for (uint32_t y = g2r.dy0; y < g2r.dy1; y++) {
        const uint32_t *row = px + (size_t)y * pitch;
        for (uint32_t x = 0; x < g2r.w; x++) {
            const uint32_t off = gs_mem_swizzle_addr32(page, g2r.fbw, x, y);
            if (off > size - 4u)
                continue;
            uint8_t *d = vram + off;
            const uint32_t c = row[x]; /* 0xRRGGBBAA */
#ifdef G2_HOST_PROF
            if (y >= g2r.uy0 && y < g2r.uy1 && g2_upload_word(d, g2r.psm) != g2_shadow[y * 1024u + x]) {
                static int n; if (n++ < 5) fprintf(stderr, "G2DBG CPU changed GPU-owned pixel fbp %x (%u,%u): uploaded %08x now %08x gpu %08x\n",
                    g2r.fbp, x, y, g2_shadow[y * 1024u + x], g2_upload_word(d, g2r.psm), c);
            }
#endif
            if (c == g2_upload_word(d, g2r.psm))
                continue; /* untouched by the GPU: keep the exact GS bytes */
            d[0] = (uint8_t)(c >> 24);
            d[1] = (uint8_t)(c >> 16);
            d[2] = (uint8_t)(c >> 8);
            if (g2r.psm != TEX_PSM_PSMCT24)
                d[3] = (uint8_t)g2_alpha_to_gs(c & 0xffu);
        }
    }
    if (g2r.zopen && g2r.zdirty && g2r.zdy1 > g2r.zdy0) {
        unsigned zpitch = 0;
        const uint32_t *zw = gx2_gs_depth_read_rows(g2r.zdy0, g2r.zdy1, &zpitch);
        if (!zw) {
            g2_stats[7]++;
            return 0;
        }
        const unsigned n = g2r.zpsm == 0u ? 4u : g2r.zpsm == 1u ? 3u : 2u;
        for (uint32_t y = g2r.zdy0; y < g2r.zdy1; y++)
            for (uint32_t x = 0; x < g2r.w; x++) {
                const uint32_t off = gs_mem_z_offset(g2r.zbp, g2r.fbw, x, y, g2r.zpsm);
                if (off == UINT32_MAX)
                    continue;
                uint8_t *p = vram + off;
                const uint32_t old = g2_read_zbytes(p, g2r.zpsm);
                const uint32_t d = gx2_gs_depth_word(zw[(size_t)y * zpitch + x]);
                if (d == g2_z_to_d24(old, g2r.zpsm))
                    continue; /* untouched: keep the exact GS value (Z32 low bits) */
                const uint32_t z = g2_d24_to_z(d, g2r.zpsm);
                for (unsigned i = 0; i < n; i++) p[i] = (uint8_t)(z >> (8u * i));
            }
        g2_stats[11]++;
    }
    g2_stats[3]++;
    g2_cause[g2_cause_now]++;
    g2_stats[9] += (uint64_t)g2r.w * (g2r.dy1 > g2r.dy0 ? g2r.dy1 - g2r.dy0 : 0u);
    G2_T1(1);
    g2r.open = g2r.zopen = g2r.zdirty = 0;
    return 1;
}

/* Make the current FRAME (and, with need_z, the current Z buffer) the GPU target. Nothing is uploaded here:
 * g2_ensure_rows() uploads the rows a draw touches. Height: up to the scissor's bottom edge, page aligned. */
static int g2_open_target(int need_z)
{
    const uint32_t fbw = g_gif.fbw, psm = g_gif.frame_psm, fbp = g_gif.fbp;
    const uint32_t zbp = g_gif.zbp, zpsm = g_gif.zpsm;
    uint32_t h = g_gif.scissor_configured ? g_gif.scissor_y1 + 1u : 512u;
    h = (h + 31u) & ~31u;
    if (h > 1024u) h = 1024u;
    const int same_fb = g2r.open && g2r.fbp == fbp && g2r.fbw == fbw && g2r.psm == psm && g2r.h >= h;
    if (same_fb && (!need_z || (g2r.zopen && g2r.zbp == zbp && g2r.zpsm == zpsm)))
        return 1;
    uint64_t zlo = 0, zhi = 0;
    if (need_z) {
        if (zbp & 2047u)
            return 0;
        zlo = (uint64_t)zbp * 4u;
        zhi = gs_page_end(zlo, fbw, fbw, same_fb ? g2r.h : h, 0x30u | zpsm);
        if (zhi > GS_MEM_SIZE || zhi <= zlo)
            return 0;
    }
    if (same_fb && !g2r.zopen) {
        /* Add the depth buffer to the open colour target. */
        if (zlo < g2r.hi && g2r.lo < zhi)
            return 0; /* overlapping colour and depth: software */
        if (!gs_mem_gpu_protect_range2((uint32_t)zlo, (uint32_t)zhi))
            return 0;
        g2r.zopen = 1;
        g2r.zdirty = 0;
        g2r.zbp = zbp;
        g2r.zpsm = zpsm;
        g2r.zlo = (uint32_t)zlo;
        g2r.zhi = (uint32_t)zhi;
        g2r.zuy0 = g2r.zuy1 = g2r.zdy0 = g2r.zdy1 = 0;
        return 1;
    }
    g2_cause_now = 1;
    if (g2r.open && !gs_mem_sync()) { /* resolve the previous target first */
        g2_cause_now = 2;
        return 0;
    }
    g2_cause_now = 2;
    if (!fbw || fbw > 1024u || (fbp & 2047u))
        return 0;
    const uint64_t lo = (uint64_t)fbp * 4u;
    const uint64_t hi = gs_page_end(lo, fbw, fbw, h, psm);
    if (hi > GS_MEM_SIZE || hi <= lo)
        return 0;
    if (need_z && zlo < hi && lo < zhi)
        return 0; /* overlapping colour and depth: software */
    if (!gx2_gs_target_cpu(fbw, h, NULL))
        return 0;
    gx2_gs_target_ready_rows(0, 0);
    memset(&g2r, 0, sizeof g2r);
    g2r.open = 1;
    g2r.zopen = need_z;
    g2r.fbp = fbp;
    g2r.fbw = fbw;
    g2r.psm = psm;
    g2r.w = fbw;
    g2r.h = h;
    g2r.lo = (uint32_t)lo;
    g2r.hi = (uint32_t)hi;
    g2r.zbp = zbp;
    g2r.zpsm = zpsm;
    g2r.zlo = (uint32_t)zlo;
    g2r.zhi = (uint32_t)zhi;
    if (!gs_mem_gpu_mark_pending() || !gs_mem_gpu_protect_range(g2r.lo, g2r.hi) ||
        (need_z && !gs_mem_gpu_protect_range2(g2r.zlo, g2r.zhi))) {
        g2r.open = g2r.zopen = 0;
        return 0;
    }
    g2_stats[2]++;
    return 1;
}

/* Upload colour rows [y0,y1) (and depth rows with z) that are not on the GPU yet. GS memory of rows the GPU never
 * drew to is still current, so it is read raw without resolving. */
static void g2_upload_color(const uint8_t *vram, uint32_t y0, uint32_t y1)
{
    unsigned pitch = 0;
    uint32_t *dst = gx2_gs_target_cpu(g2r.w, g2r.h, &pitch);
    if (!dst)
        return;
    const uint32_t page = g2r.fbp / 2048u;
    for (uint32_t y = y0; y < y1; y++) {
        uint32_t *row = dst + (size_t)y * pitch;
        for (uint32_t x = 0; x < g2r.w; x++)
        {
            row[x] = g2_upload_word(vram + gs_mem_swizzle_addr32(page, g2r.fbw, x, y), g2r.psm);
#ifdef G2_HOST_PROF
            g2_shadow[y * 1024u + x] = row[x];
#endif
        }
    }
    gx2_gs_target_ready_rows(y0, y1);
    g2_stats[8] += (uint64_t)g2r.w * (y1 - y0);
}
static void g2_upload_depth(const uint8_t *vram, uint32_t y0, uint32_t y1)
{
    unsigned zpitch = 0;
    float *zf = gx2_gs_depth_cpu(&zpitch);
    if (!zf)
        return;
    for (uint32_t y = y0; y < y1; y++)
        for (uint32_t x = 0; x < g2r.w; x++) {
            const uint32_t off = gs_mem_z_offset(g2r.zbp, g2r.fbw, x, y, g2r.zpsm);
            const uint32_t z = off == UINT32_MAX ? 0u : g2_read_zbytes(vram + off, g2r.zpsm);
            zf[(size_t)y * zpitch + x] = (float)((double)g2_z_to_d24(z, g2r.zpsm) / 16777215.0);
        }
    gx2_gs_depth_upload_rows(y0, y1);
    g2_stats[10]++;
}
static int g2_ensure_rows(uint32_t y0, uint32_t y1, int need_z)
{
    if (y1 > g2r.h) y1 = g2r.h;
    if (y0 >= y1)
        return 1;
    const uint8_t *vram = gs_mem_gpu_owner_raw();
    if (!vram)
        return 0;
    G2_T0;
    if (g2r.uy0 >= g2r.uy1) {
        g2_upload_color(vram, y0, y1);
        g2r.uy0 = y0;
        g2r.uy1 = y1;
    } else {
        if (y0 < g2r.uy0) {
            g2_upload_color(vram, y0, g2r.uy0);
            g2r.uy0 = y0;
        }
        if (y1 > g2r.uy1) { g2_upload_color(vram, g2r.uy1, y1); g2r.uy1 = y1; }
    }
    if (need_z) {
        if (g2r.zuy0 >= g2r.zuy1) {
            g2_upload_depth(vram, y0, y1);
            g2r.zuy0 = y0;
            g2r.zuy1 = y1;
        } else {
            if (y0 < g2r.zuy0) { g2_upload_depth(vram, y0, g2r.zuy0); g2r.zuy0 = y0; }
            if (y1 > g2r.zuy1) { g2_upload_depth(vram, g2r.zuy1, y1); g2r.zuy1 = y1; }
        }
    }
    G2_T1(0);
    return 1;
}
static void g2_mark_drawn(uint32_t y0, uint32_t y1, int zwrite)
{
    if (y1 > g2r.h) y1 = g2r.h;
    if (y0 >= y1)
        return;
    if (g2r.dy0 >= g2r.dy1) { g2r.dy0 = y0; g2r.dy1 = y1; }
    else {
        if (y0 < g2r.dy0) g2r.dy0 = y0;
        if (y1 > g2r.dy1) g2r.dy1 = y1;
    }
    if (zwrite) {
        g2r.zdirty = 1;
        if (g2r.zdy0 >= g2r.zdy1) { g2r.zdy0 = y0; g2r.zdy1 = y1; }
        else {
            if (y0 < g2r.zdy0) g2r.zdy0 = y0;
            if (y1 > g2r.zdy1) g2r.zdy1 = y1;
        }
    }
}

/* GS ALPHA (A,B,C,D) -> GX2 blend mode; -1 when not representable. */
static int g2_blend(void)
{
    const uint32_t a = g_gif.alpha_a, b = g_gif.alpha_b, c = g_gif.alpha_c, d = g_gif.alpha_d;
    if (d != GS_ALPHA_CD)
        return -1;
    if (a == b) /* (X - X) * C + Cd = Cd */
        return GX2_GS_BLEND_KEEP;
    if (a == GS_ALPHA_CS && b == GS_ALPHA_CD)
        return c == GS_ALPHA_AS ? GX2_GS_BLEND_MIX_AS : c == GS_ALPHA_AFIX ? GX2_GS_BLEND_MIX_FIX : GX2_GS_BLEND_MIX_AD;
    if (a == GS_ALPHA_CS && b == GS_ALPHA_ZERO) {
        if (c == GS_ALPHA_AS) return GX2_GS_BLEND_ADD_AS;
        if (c == GS_ALPHA_AD) return GX2_GS_BLEND_ADD_AD;
        return g_gif.alpha_fix == 0x80u ? GX2_GS_BLEND_ADD : g_gif.alpha_fix == 0u ? GX2_GS_BLEND_KEEP : GX2_GS_BLEND_ADD_FIX;
    }
    if (a == GS_ALPHA_ZERO && b == GS_ALPHA_CS) {
        if (c == GS_ALPHA_AS) return GX2_GS_BLEND_SUB_AS;
        if (c == GS_ALPHA_AFIX) return GX2_GS_BLEND_SUB_FIX;
        return -1;
    }
    return -1;
}

/* Depth state for this draw: *func = GX2CompareFunction or -1, *write. Returns 0 when depth is not
 * representable, 1 otherwise (need_z set when the draw tests or writes Z). */
static int g2_depth_state(int *func, int *write, int *need_z)
{
    *func = -1;
    *write = 0;
    *need_z = 0;
    if (!g_gif.zbuf_configured)
        return 1;
    const int zwrite = !g_gif.zmsk;
    const int ztest = g_gif.zte && g_gif.ztst != GS_ZTST_ALWAYS;
    if (!zwrite && !ztest)
        return 1;
    if (!gx2_gs_depth_ready())
        return 0;
    if (g_gif.zpsm != 0u && g_gif.zpsm != 1u && g_gif.zpsm != 2u && g_gif.zpsm != 10u)
        return 0;
    if (ztest)
        *func = g_gif.ztst == GS_ZTST_NEVER ? 0 : g_gif.ztst == GS_ZTST_GEQUAL ? 6 : g_gif.ztst == GS_ZTST_GREATER ? 4 : 7;
    *write = zwrite;
    *need_z = 1;
    return 1;
}

/* Common state gate. Returns the blend mode (>= 0) or -1. */
static int g2_state_ok(uint32_t attr, int textured, int *zfunc, int *zwrite, int *need_z)
{
    if (!gekko2_gx2_gs_enabled || !gx2_gs_ready())
        return -1;
    if (g_gif.frame_psm != TEX_PSM_PSMCT32 && g_gif.frame_psm != TEX_PSM_PSMCT24)
        G2_REJECT(0);
    if (g_gif.fbmask || g_gif.fba || g_gif.date || gs_dither_active() || (attr & PRIM_FGE_MASK) || g_gif.scanmsk)
        G2_REJECT(1);
    if (!g2_depth_state(zfunc, zwrite, need_z))
        G2_REJECT(2);
    if (g_gif.ate && g_gif.atst != GS_ATST_ALWAYS)
        G2_REJECT(3);
    if (g_gif.colclamp_configured && !g_gif.colclamp)
        G2_REJECT(4);
    int blend = GX2_GS_BLEND_OFF;
    if (attr & PRIM_ABE_MASK) {
        if (g_gif.pabe)
            G2_REJECT(5);
        blend = g2_blend();
        if (blend < 0)
            G2_REJECT(6);
    }
    if (textured) {
        if (g_gif.tex_tfx != TEX_TFX_MODULATE && g_gif.tex_tfx != TEX_TFX_DECAL)
            G2_REJECT(7);
        if (g_gif.tex1_mxl && g_gif.tex1_mmin > 1u)
            G2_REJECT(8);
        if (g_gif.tex_tw > 10u || g_gif.tex_th > 10u)
            G2_REJECT(9);
        if (g_gif.clamp_configured && (g_gif.clamp_wms > 1u || g_gif.clamp_wmt > 1u || g_gif.clamp_wms != g_gif.clamp_wmt))
            G2_REJECT(10);
    }
    return blend;
}

/* Decode the current texture (TEX0/CLUT/TEXA) to 0xAARRGGBB with GS alpha and upload it, unless the same
 * texture with the same GS memory contents is already in the slot. */
static uint32_t g2_tex_buf[1024u * 1024u];
/* Texel rectangle a primitive can sample: [x0,x1) x [y0,y1). Bilinear filtering and the GPU's half-texel
 * rounding read one more texel on each side. Wrapped coordinates (REPEAT outside 0..size) use the whole texture. */
typedef struct { uint32_t x0, y0, x1, y1; } g2_rect;
static g2_rect g2_tex_rect(const double *u, const double *v, unsigned n)
{
    const uint32_t tw = 1u << g_gif.tex_tw, th = 1u << g_gif.tex_th;
    g2_rect r = { 0, 0, tw, th };
    double umin = u[0], umax = u[0], vmin = v[0], vmax = v[0];
    for (unsigned i = 1; i < n; i++) {
        if (u[i] < umin) umin = u[i];
        if (u[i] > umax) umax = u[i];
        if (v[i] < vmin) vmin = v[i];
        if (v[i] > vmax) vmax = v[i];
    }
    if (!(umin >= -1.0 && vmin >= -1.0 && umax <= (double)tw + 1.0 && vmax <= (double)th + 1.0))
        return r; /* wraps or clamps outside: whole texture */
    /* Point sampling reads floor(u) for u in [umin, umax); bilinear also the neighbour half a texel away. */
    const double m = g_gif.tex1_mmag ? 0.5 : 0.0;
    const int32_t x0 = (int32_t)floor(umin - m), y0 = (int32_t)floor(vmin - m);
    const int32_t x1 = (int32_t)ceil(umax + m), y1 = (int32_t)ceil(vmax + m);
    r.x0 = x0 < 0 ? 0u : (uint32_t)x0;
    r.y0 = y0 < 0 ? 0u : (uint32_t)y0;
    r.x1 = x1 > (int32_t)tw ? tw : (uint32_t)x1;
    r.y1 = y1 > (int32_t)th ? th : (uint32_t)y1;
    if (r.x1 <= r.x0 || r.y1 <= r.y0) { r.x0 = r.y0 = 0; r.x1 = tw; r.y1 = th; }
    return r;
}
static int g2_overlaps_target(uint64_t lo, uint64_t hi)
{
    if (!g2r.open)
        return 0;
    if (lo < g2r.hi && g2r.lo < hi)
        return 1;
    return g2r.zopen && lo < g2r.zhi && g2r.zlo < hi;
}
static int g2_bind_texture_impl(g2_rect rc);
static int g2_bind_texture(g2_rect rc)
{
    g2_cause_now = 0;
    const int r = g2_bind_texture_impl(rc);
    g2_cause_now = 2;
    return r;
}
static int g2_bind_texture_impl(g2_rect rc)
{
    const uint32_t tw = 1u << g_gif.tex_tw, th = 1u << g_gif.tex_th;
    const uint64_t base = (uint64_t)g_gif.tex_tbp0 * 256u;
    const uint32_t tbw = g_gif.tex_tbw ? g_gif.tex_tbw : 64u;
    const uint32_t linear = g_gif.tex1_mmag ? 1u : 0u;
    const uint32_t wrap = !g_gif.clamp_configured || g_gif.clamp_wms == 0u;
    /* Texture identity without the rectangle: TEX0/TEXA/CLUT state (and the CLUT contents for indexed formats). */
    const uint32_t state[] = { g_gif.tex_tbp0, g_gif.tex_tbw, g_gif.tex_tw, g_gif.tex_th, g_gif.tex_psm, g_gif.tex_cbp,
                               g_gif.tex_cpsm, g_gif.tex_csa, g_gif.tex_csm, g_gif.tex_tcc, g_gif.texa_ta0,
                               g_gif.texa_ta1, g_gif.texa_aem, g_gif.clut_words_valid ? g_gif.clut_write_csm : 2u };
    uint32_t lo = 2166136261u, hi = 0;
    for (unsigned k = 0; k < sizeof state / sizeof state[0]; k++) {
        lo = (lo ^ state[k]) * 16777619u;
        hi = ((hi << 5) | (hi >> 27)) ^ state[k];
    }
    const int paletted = g_gif.tex_psm == TEX_PSM_PSMT8 || g_gif.tex_psm == TEX_PSM_PSMT4 || g_gif.tex_psm == TEX_PSM_PSMT8H ||
                         g_gif.tex_psm == TEX_PSM_PSMT4HL || g_gif.tex_psm == TEX_PSM_PSMT4HH;
    if (paletted)
        for (unsigned k = 0; k < 512u; k++) {
            lo = (lo ^ gs_clut_hash_word(k)) * 16777619u;
            hi = ((hi << 5) | (hi >> 27)) ^ gs_clut_hash_word(k);
        }
    /* Same texture still in the slot and its decoded rectangle's GS memory unchanged? */
    /* The texture memory lives in a ring that is reused after it wraps (gx2_gs_texture_epoch changes): a texture
     * uploaded before the wrap may have been overwritten, so it is uploaded again. */
    int same = g2t.valid && g2t.epoch == gx2_gs_texture_epoch() && g2t.lo == lo && g2t.hi == hi && g2t.w == tw &&
               g2t.h == th;

    if (same && g2_overlaps_target(base, gs_page_end(base, tbw, g2t.rx1, g2t.ry1, g_gif.tex_psm)))
        same = 0; /* checking the old area would resolve the GPU target: start over with this rectangle */
    if (same) {
        const uint64_t e = gs_page_end(base, tbw, g2t.rx1, g2t.ry1, g_gif.tex_psm);
        uint64_t hv = 0;
        /* A texture read overlapping the GPU target resolves it here (frame buffer feedback effects). */
        if (e > GS_MEM_SIZE || e <= base || !gs_mem_gpu_sync_alias_range((uint32_t)base, (uint32_t)e) ||
            !gs_mem_hash_range((uint32_t)base, (uint32_t)(e - base), &hv))
            return 0;
        same = hv == g2t.data;
    }
    if (same && rc.x0 >= g2t.rx0 && rc.y0 >= g2t.ry0 && rc.x1 <= g2t.rx1 && rc.y1 <= g2t.ry1) {
        if (g2t.linear != linear || g2t.wrap != wrap) {
            gx2_gs_rebind_texture((int)linear, (int)wrap);
            g2t.linear = linear;
            g2t.wrap = wrap;
        }
        g2_stats[6]++;
        return 1;
    }
    /* Decode and upload only the new rectangle; with the same texture still in the slot the decoded area grows. */
    g2_rect u = rc;
    if (same) {
        if (g2t.rx0 < u.x0) u.x0 = g2t.rx0;
        if (g2t.ry0 < u.y0) u.y0 = g2t.ry0;
        if (g2t.rx1 > u.x1) u.x1 = g2t.rx1;
        if (g2t.ry1 > u.y1) u.y1 = g2t.ry1;
        if (g2_overlaps_target(base, gs_page_end(base, tbw, u.x1, u.y1, g_gif.tex_psm)))
            u = rc;

    }
    const uint64_t e = gs_page_end(base, tbw, u.x1, u.y1, g_gif.tex_psm);
    uint64_t hv = 0;
    if (e > GS_MEM_SIZE || e <= base || !gs_mem_gpu_sync_alias_range((uint32_t)base, (uint32_t)e) ||
        !gs_mem_hash_range((uint32_t)base, (uint32_t)(e - base), &hv))
        return 0;
    /* The union's bounding box can contain texels neither rectangle covered: decode all of it. */
    G2_T0;
    for (uint32_t y = u.y0; y < u.y1; y++)
        for (uint32_t x = u.x0; x < u.x1; x++) {
            const uint32_t t = gs_sample_texel_uncached((int32_t)x, (int32_t)y); /* R | G<<8 | B<<16 | A<<24 */
            g2_tex_buf[y * tw + x] = (t & 0xff000000u) | ((t & 0xffu) << 16) | (t & 0xff00u) | ((t >> 16) & 0xffu);
        }
    G2_T1(2);
    /* TEX0.TCC = 0: the texel alpha is not used (RGB texture), the vertex alpha alone decides. */
    if (!gx2_gs_set_texture_rect(g2_tex_buf, tw, th, u.x0, u.y0, u.x1, u.y1, g_gif.tex_tcc ? 1 : 0, (int)linear, (int)wrap))
        return 0;
    g2t.valid = 1;
    g2t.epoch = gx2_gs_texture_epoch();
    g2t.lo = lo;
    g2t.hi = hi;
    g2t.w = tw;
    g2t.h = th;
    g2t.rx0 = u.x0;
    g2t.ry0 = u.y0;
    g2t.rx1 = u.x1;
    g2t.ry1 = u.y1;
    g2t.data = hv;
    g2t.linear = linear;
    g2t.wrap = wrap;
    g2_stats[5]++;
    return 1;
}

static void g2_apply_scissor(void)
{
    if (g_gif.scissor_configured)
        gx2_gs_set_scissor(g_gif.scissor_x0, g_gif.scissor_y0, g_gif.scissor_x1 - g_gif.scissor_x0 + 1u,
                           g_gif.scissor_y1 - g_gif.scissor_y0 + 1u);
    else
        gx2_gs_set_scissor(0, 0, 4096, 4096);
}

/* Vertex colour for the shader: GS 0x80 = 1.0. DECAL: texel colour unchanged (vertex RGB 1.0); with TCC the
 * texel alpha is used as is (vertex alpha 1.0). */
static void g2_color(float out[4], uint32_t rgba, int textured)
{
    out[0] = (float)rgba_channel(rgba, 0) / 128.0f;
    out[1] = (float)rgba_channel(rgba, 8) / 128.0f;
    out[2] = (float)rgba_channel(rgba, 16) / 128.0f;
    out[3] = (float)rgba_channel(rgba, 24) / 128.0f;
    if (textured && g_gif.tex_tfx == TEX_TFX_DECAL) {
        out[0] = out[1] = out[2] = 1.0f;
        if (g_gif.tex_tcc)
            out[3] = 1.0f;
    }
}

/* Window position (GS pixel centres are integers, GPU centres at +0.5) to clip space; w for STQ perspective;
 * depth normalised 0..1. */
static void g2_pos(float out[4], double x, double y, float w, float depth)
{
    unsigned tw = 0, th = 0;
    gx2_gs_size(&tw, &th);
    const float cx = (float)((x + 0.5) / (double)tw * 2.0 - 1.0);
    const float cy = (float)(1.0 - (y + 0.5) / (double)th * 2.0);
    out[0] = cx * w;
    out[1] = cy * w;
    out[2] = depth * w;
    out[3] = w;
}
static float g2_depth_of(uint32_t z, int need_z)
{
    if (!need_z)
        return 0.0f;
    uint32_t zz = gs_mem_clamp_raster_z(z, g_gif.zpsm);
    return (float)((double)g2_z_to_d24(zz, g_gif.zpsm) / 16777215.0);
}

/* Rows a primitive with these window y extents can touch, clipped to the scissor; [*r0,*r1). */
static void g2_rows(double ymin, double ymax, uint32_t *r0, uint32_t *r1)
{
    double a = floor(ymin) - 1.0, b = ceil(ymax) + 1.0;
    if (g_gif.scissor_configured) {
        if (a < (double)g_gif.scissor_y0) a = (double)g_gif.scissor_y0;
        if (b > (double)g_gif.scissor_y1 + 1.0) b = (double)g_gif.scissor_y1 + 1.0;
    }
    if (a < 0.0) a = 0.0;
    if (b < a) b = a;
    if (b > 1024.0) b = 1024.0;
    *r0 = (uint32_t)a;
    *r1 = (uint32_t)b;
}
static int g2_prepare(uint32_t attr, int textured, g2_rect rc, uint32_t ry0, uint32_t ry1, int *blend_out, int *need_z_out)
{
    int zfunc = -1, zwrite = 0, need_z = 0;
    const int blend = g2_state_ok(attr, textured, &zfunc, &zwrite, &need_z);
    if (blend < 0) {
        if (gekko2_gx2_gs_enabled) g2_stats[4]++;
        return 0;
    }
    if (textured && !g2_bind_texture(rc)) { /* may resolve the target (feedback): bind before opening it */
        g2_reasons[12]++;
        return 0;
    }
    if (!g2_open_target(need_z)) {
        g2_reasons[13]++;
        return 0;
    }
    /* No second bind: the texture was decoded from resolved memory before the target opened, and opening the
     * target restores the texture slot binding. Re-reading it could resolve the target we are drawing into. */
    if (!g2_ensure_rows(ry0, ry1, need_z)) {
        g2_reasons[13]++;
        return 0;
    }
    g2_apply_scissor();
    gx2_gs_set_blend(blend, g_gif.alpha_fix);
    gx2_gs_set_depth(zfunc, zwrite);
    g2_mark_drawn(ry0, ry1, zwrite);
    *blend_out = blend;
    *need_z_out = need_z;
    return 1;
}

static int g2_try_triangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                           uint32_t c0, uint32_t c1, uint32_t c2,
                           double u0, double v0, double u1, double v1, double u2, double v2,
                           float s0, float t0, float q0, float s1, float t1, float q1, float s2, float t2, float q2,
                           uint32_t z0, uint32_t z1, uint32_t z2)
{
    const uint32_t attr = gs_effective_attr_prim();
    const int textured = (attr & PRIM_TME_MASK) != 0;
    const int fst = (attr & PRIM_FST_MASK) != 0;
    if (textured && !fst && !(q0 > 0.0f && q1 > 0.0f && q2 > 0.0f)) {
        g2_reasons[11]++;
        return 0;
    }
    g2_rect rc = { 0, 0, 1, 1 };
    if (textured) {
        double tu[3], tv[3];
        const float sq[3] = { s0, s1, s2 }, tq[3] = { t0, t1, t2 }, qq[3] = { q0, q1, q2 };
        const double uu[3] = { u0, u1, u2 }, vv[3] = { v0, v1, v2 };
        for (int i = 0; i < 3; i++) {
            tu[i] = fst ? uu[i] : (double)(sq[i] / qq[i]) * (double)(1u << g_gif.tex_tw);
            tv[i] = fst ? vv[i] : (double)(tq[i] / qq[i]) * (double)(1u << g_gif.tex_th);
        }
        rc = g2_tex_rect(tu, tv, 3);
    }
    int blend = 0, need_z = 0;
    uint32_t ry0, ry1;
    {
        int32_t mn = y0, mx = y0;
        if (y1 < mn) mn = y1;
        if (y1 > mx) mx = y1;
        if (y2 < mn) mn = y2;
        if (y2 > mx) mx = y2;
        g2_rows((double)mn, (double)mx, &ry0, &ry1);
    }
    if (ry0 >= ry1)
        return 1; /* entirely outside the scissor: nothing to draw */
    if (!g2_prepare(attr, textured, rc, ry0, ry1, &blend, &need_z))
        return 0;
    const int gouraud = (attr & PRIM_IIP_MASK) != 0;
    const int32_t xs[3] = { x0, x1, x2 }, ys[3] = { y0, y1, y2 };
    const uint32_t cs[3] = { gouraud ? c0 : c2, gouraud ? c1 : c2, c2 }, zs[3] = { z0, z1, z2 };
    const double us[3] = { u0, u1, u2 }, vs[3] = { v0, v1, v2 };
    const float ss[3] = { s0, s1, s2 }, ts[3] = { t0, t1, t2 }, qs[3] = { q0, q1, q2 };
    const double tw = (double)(1u << g_gif.tex_tw), th = (double)(1u << g_gif.tex_th);
    gx2_gs_vertex v[3];
    for (int i = 0; i < 3; i++) {
        float w = 1.0f;
        if (textured && !fst) {
            w = 1.0f / qs[i];
            v[i].uv[0] = ss[i] / qs[i];
            v[i].uv[1] = ts[i] / qs[i];
        } else if (textured) {
            v[i].uv[0] = (float)(us[i] / tw);
            v[i].uv[1] = (float)(vs[i] / th);
        } else {
            v[i].uv[0] = v[i].uv[1] = 0.0f;
        }
        v[i].uv[2] = 0.0f;
        v[i].uv[3] = 1.0f;
        g2_pos(v[i].pos, (double)xs[i], (double)ys[i], w, g2_depth_of(zs[i], need_z));
        g2_color(v[i].col, cs[i], textured);
    }
    G2_T0;
    if (!gx2_gs_draw(v, 3, GX2_GS_TRIANGLES, textured))
        return 0;
    G2_T1(3);
    if (textured)
        gx2_gs_mark_texture_used();
    g2_stats[0]++;
    g_gif.triangles_drawn++;
    return 1;
}

static int g2_try_sprite(int32_t raw_x0, int32_t raw_y0, int32_t raw_x1, int32_t raw_y1,
                         double u0, double v0, double u1, double v1,
                         float s0, float t0, float q0, float s1, float t1, float q1, uint32_t z1)
{
    const uint32_t attr = gs_effective_attr_prim();
    const int textured = (attr & PRIM_TME_MASK) != 0;
    const int fst = (attr & PRIM_FST_MASK) != 0;
    if (textured && !fst && !(q0 > 0.0f && q1 > 0.0f)) {
        g2_reasons[11]++;
        return 0;
    }
    g2_rect rc = { 0, 0, 1, 1 };
    if (textured) {
        const double tu[2] = { fst ? u0 : (double)(s0 / q0) * (double)(1u << g_gif.tex_tw),
                               fst ? u1 : (double)(s1 / q1) * (double)(1u << g_gif.tex_tw) };
        const double tv[2] = { fst ? v0 : (double)(t0 / q0) * (double)(1u << g_gif.tex_th),
                               fst ? v1 : (double)(t1 / q1) * (double)(1u << g_gif.tex_th) };
        rc = g2_tex_rect(tu, tv, 2);
    }
    int blend = 0, need_z = 0;
    uint32_t ry0, ry1;
    g2_rows(raw_y0 < raw_y1 ? raw_y0 / 16.0 : raw_y1 / 16.0, raw_y0 < raw_y1 ? raw_y1 / 16.0 : raw_y0 / 16.0, &ry0, &ry1);
    if (ry0 >= ry1)
        return 1;
    if (!g2_prepare(attr, textured, rc, ry0, ry1, &blend, &need_z))
        return 0;
    const double px0 = raw_x0 / 16.0, py0 = raw_y0 / 16.0, px1 = raw_x1 / 16.0, py1 = raw_y1 / 16.0;
    const double tw = (double)(1u << g_gif.tex_tw), th = (double)(1u << g_gif.tex_th);
    float ua = 0, va = 0, ub = 0, vb = 0;
    if (textured) {
        if (fst) {
            ua = (float)(u0 / tw); va = (float)(v0 / th);
            ub = (float)(u1 / tw); vb = (float)(v1 / th);
        } else {
            ua = s0 / q0; va = t0 / q0;
            ub = s1 / q1; vb = t1 / q1;
        }
    }
    /* Corners (x0,y0) (x1,y0) (x1,y1) (x0,y1); GS sprites are flat with the second vertex's colour and Z.
     * GS coverage [ceil(p0), ceil(p1)) == GPU centres x+0.5 inside [p0+0.5, p1+0.5): g2_pos adds 0.5. */
    const double xs[4] = { px0, px1, px1, px0 }, ys[4] = { py0, py0, py1, py1 };
    const float us[4] = { ua, ub, ub, ua }, vs[4] = { va, va, vb, vb };
    const float depth = g2_depth_of(z1, need_z);
    gx2_gs_vertex v[4];
    for (int i = 0; i < 4; i++) {
        g2_pos(v[i].pos, xs[i], ys[i], 1.0f, depth);
        g2_color(v[i].col, g_gif.rgba, textured);
        v[i].uv[0] = us[i];
        v[i].uv[1] = vs[i];
        v[i].uv[2] = 0.0f;
        v[i].uv[3] = 1.0f;
    }
    G2_T0;
    if (!gx2_gs_draw(v, 4, GX2_GS_QUADS, textured))
        return 0;
    G2_T1(3);
    if (textured)
        gx2_gs_mark_texture_used();
    g2_stats[1]++;
    return 1;
}

/* Called by the run loop after gx2_gs_init(); binds the resolver. */
int gekko2_gx2_gs_attach(void)
{
    if (!gx2_gs_ready())
        return 0;
    if (!gs_mem_gpu_bind(g2_resolver, NULL))
        return 0;
    memset(&g2r, 0, sizeof g2r);
    memset(&g2t, 0, sizeof g2t);
    gekko2_gx2_gs_enabled = 1;
    return 1;
}
/* Resolve everything and stop using the GPU (end of emulation). */
void gekko2_gx2_gs_detach(void)
{
    if (gekko2_gx2_gs_enabled)
        gs_mem_sync();
    gekko2_gx2_gs_enabled = 0;
    g2r.open = g2r.zopen = 0;
    g2t.valid = 0;
}
