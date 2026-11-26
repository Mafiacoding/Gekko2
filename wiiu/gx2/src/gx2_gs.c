/* GPL-3.0-or-later. See gx2_gs.h. */
#include "gx2_gs.h"
#include <coreinit/cache.h>
#include <coreinit/memdefaultheap.h>
#include <gx2/clear.h>
#include <gx2/context.h>
#include <gx2/draw.h>
#include <gx2/enum.h>
#include <gx2/event.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/sampler.h>
#include <gx2/shaders.h>
#include <gx2/state.h>
#include <gx2/surface.h>
#include <gx2/texture.h>
#include <whb/gfx.h>
#include <string.h>
#include <stdio.h>

/* Not declared by wut's headers; exported by gx2.rpl. */
void GX2ConvertDepthBufferToTextureSurface(GX2DepthBuffer *src, GX2Surface *dst, uint32_t mipLevel, uint32_t slice);

#define VB_MAX 49152u /* vertices per GPU wait; 48 bytes each */

static WHBGfxShaderGroup sh_color, sh_mod, sh_fbc; /* sh_fbc: gs_fbcopy.psh, optional */
static GX2ContextState *ctx;
static GX2ColorBuffer target;
static GX2Texture tex;
static GX2Sampler samplers[2][2]; /* [linear][wrap] */
static gx2_gs_vertex *vb;
static unsigned vb_used, vb_flushed;
static int ready, tex_pending_use, cur_blend = -1;
static unsigned tw_max = 1024, th_max = 1024;
static unsigned cur_w, cur_h, tex_linear, tex_wrap, tex_w_used, tex_h_used; static int tex_valid;
static uint64_t stats[5];
static GX2Texture tex_view;
static float tex_vscale = 1.0f;
int gekko2_wiiu_rb_check;
int gekko2_wiiu_upload_rle;
uint64_t gekko2_wiiu_rb_stats[2];
/* GX2 state cache (idea from the PPSSPP Wii U GX2 backend, StateMappingGX2.cpp; re-implemented): the GS draws set
 * the same shaders and scissor over and over. These remember what is on the GPU; any other user of the GPU state
 * (bind_state, uploads, depth passes, the presenter via ctx_lost) clears them. */
static const void *st_shader;
static unsigned st_sc[4] = { ~0u, ~0u, ~0u, ~0u };
static void state_forget(void) { st_shader = NULL; st_sc[0] = ~0u; }
int tex_pad_rows; /* diagnostics: tex_pad.txt */
static int view_ok;
/* Texture ring: every CPU texture upload (GS textures, colour rows, depth rows) gets fresh memory from a ring.
 * Nothing the GPU may still read is overwritten, so no wait is needed when the texture changes, and Cemu, which
 * re-checks a texture's memory at most once per frame, always meets a new address. Cemu also recomputes the pitch
 * of linear textures itself, so every texture uses exactly the pitch GX2CalcSurfaceSizeAndAlignment gives. */
#define RING_BYTES (24u << 20)
static uint8_t *ring;
static uint32_t ring_off;
static uint64_t ring_wraps;
#define FB_RING_BYTES (8u << 20) /* GPU-written feedback textures (gx2_gs_texture_from_target) */
static uint8_t *fb_ring;
static uint32_t fb_ring_off;
static void gpu_wait(void);
static void *ring_get(uint32_t size, uint32_t align)
{
    if (align < 256u) align = 256u;
    uint32_t off = (ring_off + align - 1u) & ~(align - 1u);
    if (size > RING_BYTES)
        return NULL;
    if (off + size > RING_BYTES) {
        gpu_wait(); /* the GPU is done with everything in the ring */
        ring_wraps++;
        /* The bound GS texture stays usable: it lies near the end of the ring, which the next round reaches only much
         * later, and gx2_gs_texture_epoch() makes the GS texture cache upload it again before its next use. (Dropping it
         * here left the colour-row upload texture bound, which then drew picture rows at wrong places.) */
        off = 0;
    }
    ring_off = off + size;
    return ring + off;
}
/* dcbz before filling whole cache lines of upload memory: the CPU then does not first read the old ring contents
 * from RAM (technique used for the GX2 upload buffers of the PPSSPP Wii U port, jereif/PPSSPP_wiiu
 * DrawEngineGX2.cpp, GPL-2.0-or-later; re-implemented). Only for 32-byte aligned ranges that are written completely.
 * sd:/wiiu/gekko2/dcbz_off.txt turns it off. */
int gekko2_wiiu_dcbz = 1;
static void dcbz_range(void *p, size_t bytes)
{
    if (!gekko2_wiiu_dcbz || ((uintptr_t)p & 31u))
        return;
    for (uint8_t *q = (uint8_t *)p, *e = q + (bytes & ~(size_t)31u); q < e; q += 32)
        __asm__ volatile("dcbz 0,%0" : : "r"(q) : "memory");
}
static void *tex_in_ring(GX2Texture *t, unsigned w, unsigned h, GX2SurfaceFormat fmt)
{
    memset(t, 0, sizeof *t);
    t->surface.width = w;
    t->surface.height = h;
    t->surface.depth = 1;
    t->surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    t->surface.format = fmt;
    t->surface.use = GX2_SURFACE_USE_TEXTURE;
    t->surface.mipLevels = 1;
    t->surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    t->viewNumSlices = 1;
    t->compMap = 0x00010203;
    GX2CalcSurfaceSizeAndAlignment(&t->surface);
    t->surface.image = ring_get(t->surface.imageSize, t->surface.alignment);
    if (!t->surface.image)
        return NULL;
    GX2InitTextureRegs(t);
    return t->surface.image;
}
/* CPU staging for colour and depth rows (plain memory; copied into the ring on upload). */
static uint32_t *cstage;
static unsigned cstage_pitch;
static GX2Texture up_tex;
static GX2Texture zconv_tex;
static GX2ColorBuffer zrb;
static int z_read_shader, z_read_swap;
static int ctx_lost;
/* Depth: D24_S8 UNORM depth buffer (format code 0x011), FLOAT_R32 upload texture, converted + linear copies for
 * readback. Endianness of the readback word and of the uploaded floats is calibrated at init (z_word_mode,
 * z_upload_swap) because it could not be checked on hardware by the author. */
static WHBGfxShaderGroup sh_zload;
static GX2DepthBuffer zbuf;
static GX2Texture ztex;
static GX2Surface zconv, zlin;
static int zready, z_word_mode, z_upload_swap, cur_zfunc = -1, cur_zwrite;
static char z_status[256] = "depth not initialised";
void gx2_gs_context_lost(void) { ctx_lost = 1; }

uint64_t gx2_gs_stat(unsigned i) { return i < 5 ? stats[i] : 0; }
int gx2_gs_ready(void) { return ready; }

static int load_group(WHBGfxShaderGroup *g, const void *gsh)
{
    memset(g, 0, sizeof *g);
    if (!WHBGfxLoadGFDShaderGroup(g, 0, gsh))
        return 0;
    if (!WHBGfxInitShaderAttribute(g, "aPosition", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32) ||
        !WHBGfxInitShaderAttribute(g, "aColor", 0, 16, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32) ||
        !WHBGfxInitShaderAttribute(g, "aTexCoord", 0, 32, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32) ||
        !WHBGfxInitFetchShader(g))
        return 0;
    return 1;
}

int gx2_gs_init(const void *gsh_color, const void *gsh_modulate, unsigned width, unsigned height)
{
    if (ready || !gsh_color || !gsh_modulate || !width || !height || width > 2048 || height > 2048)
        return 0;
    if (!load_group(&sh_color, gsh_color) || !load_group(&sh_mod, gsh_modulate))
        goto fail;

    ctx = MEMAllocFromDefaultHeapEx(sizeof(GX2ContextState), GX2_CONTEXT_STATE_ALIGNMENT);
    if (!ctx)
        goto fail;
    GX2SetupContextStateEx(ctx, TRUE);

    memset(&target, 0, sizeof target);
    target.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    target.surface.width = width;
    target.surface.height = height;
    target.surface.depth = 1;
    target.surface.mipLevels = 1;
    target.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    target.surface.aa = GX2_AA_MODE1X;
    target.surface.use = GX2_SURFACE_USE_COLOR_BUFFER;
    target.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED; /* CPU-readable without detiling */
    target.viewNumSlices = 1;
    GX2CalcSurfaceSizeAndAlignment(&target.surface);
    GX2InitColorBufferRegs(&target);
    target.surface.image = MEMAllocFromDefaultHeapEx(target.surface.imageSize, target.surface.alignment);
    if (!target.surface.image)
        goto fail;
    memset(target.surface.image, 0, target.surface.imageSize);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU, target.surface.image, target.surface.imageSize);

    ring = MEMAllocFromDefaultHeapEx(RING_BYTES, 0x1000);
    if (!ring)
        goto fail;
    ring_off = 0;
    cstage_pitch = target.surface.width;
    cstage = MEMAllocFromDefaultHeapEx(cstage_pitch * target.surface.height * 4u, 64);
    if (!cstage)
        goto fail;

    for (int l = 0; l < 2; l++)
        for (int w = 0; w < 2; w++)
            GX2InitSampler(&samplers[l][w], w ? GX2_TEX_CLAMP_MODE_WRAP : GX2_TEX_CLAMP_MODE_CLAMP,
                           l ? GX2_TEX_XY_FILTER_MODE_LINEAR : GX2_TEX_XY_FILTER_MODE_POINT);

    vb = MEMAllocFromDefaultHeapEx(VB_MAX * sizeof(gx2_gs_vertex), GX2_VERTEX_BUFFER_ALIGNMENT);
    if (!vb)
        goto fail;
    vb_used = vb_flushed = 0;
    ready = 1;
    return 1;
fail:
    gx2_gs_shutdown();
    return 0;
}

/* Optional frame buffer feedback copy shader (gs.vsh + gs_fbcopy.psh). Without it the copy uses the modulate shader
 * and cannot produce PSMCT24 TEXA.AEM alpha. */
int gx2_gs_init_fbcopy(const void *gsh)
{
    if (!ready || !gsh || sh_fbc.pixelShader)
        return sh_fbc.pixelShader != NULL;
    if (!load_group(&sh_fbc, gsh)) {
        memset(&sh_fbc, 0, sizeof sh_fbc);
        return 0;
    }
    return 1;
}
int gx2_gs_fbcopy_ready(void) { return sh_fbc.pixelShader != NULL; }

void gx2_gs_shutdown(void)
{
    if (ready)
        GX2DrawDone();
    if (vb) MEMFreeToDefaultHeap(vb);
    if (ring) MEMFreeToDefaultHeap(ring);
    ring = NULL;
    if (fb_ring) MEMFreeToDefaultHeap(fb_ring);
    fb_ring = NULL;
    if (cstage) MEMFreeToDefaultHeap(cstage);
    cstage = NULL;
    if (target.surface.image) MEMFreeToDefaultHeap(target.surface.image);
    if (ctx) MEMFreeToDefaultHeap(ctx);
    vb = NULL;
    ctx = NULL;
    cur_w = cur_h = 0;
    tex_valid = 0;
    view_ok = 0;
    memset(&tex, 0, sizeof tex);
    memset(&target, 0, sizeof target);
    if (sh_color.vertexShader || sh_color.pixelShader) WHBGfxFreeShaderGroup(&sh_color);
    if (sh_mod.vertexShader || sh_mod.pixelShader) WHBGfxFreeShaderGroup(&sh_mod);
    if (sh_fbc.vertexShader || sh_fbc.pixelShader) WHBGfxFreeShaderGroup(&sh_fbc);
    memset(&sh_fbc, 0, sizeof sh_fbc);
    memset(&sh_color, 0, sizeof sh_color);
    memset(&sh_mod, 0, sizeof sh_mod);
    if (zbuf.surface.image) MEMFreeToDefaultHeap(zbuf.surface.image);
    if (ztex.surface.image) MEMFreeToDefaultHeap(ztex.surface.image);
    if (zconv.image) MEMFreeToDefaultHeap(zconv.image);
    if (zlin.image) MEMFreeToDefaultHeap(zlin.image);
    if (zrb.surface.image) MEMFreeToDefaultHeap(zrb.surface.image);
    zrb.surface.image = NULL;
    z_read_shader = z_read_swap = 0;
    if (sh_zload.vertexShader || sh_zload.pixelShader) WHBGfxFreeShaderGroup(&sh_zload);
    memset(&zbuf, 0, sizeof zbuf);
    memset(&ztex, 0, sizeof ztex);
    memset(&zconv, 0, sizeof zconv);
    memset(&zlin, 0, sizeof zlin);
    memset(&sh_zload, 0, sizeof sh_zload);
    zready = 0;
    cur_zfunc = -1;
    cur_zwrite = 0;
    ready = 0;
}

static void bind_state(void)
{
    state_forget();
    ctx_lost = 0;
    GX2SetContextState(ctx);
    GX2SetColorBuffer(&target, GX2_RENDER_TARGET_0);
    if (!cur_w || !cur_h) {
        cur_w = target.surface.width;
        cur_h = target.surface.height;
    }
    GX2SetViewport(0.0f, 0.0f, (float)cur_w, (float)cur_h, 0.0f, 1.0f);
    GX2SetScissor(0, 0, cur_w, cur_h);
    if (zready)
        GX2SetDepthBuffer(&zbuf);
    if (zready && (cur_zfunc >= 0 || cur_zwrite))
        GX2SetDepthOnlyControl(TRUE, cur_zwrite ? TRUE : FALSE,
                               cur_zfunc >= 0 ? (GX2CompareFunction)cur_zfunc : GX2_COMPARE_FUNC_ALWAYS);
    else
        GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
    GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, FALSE, FALSE);
    cur_blend = -1;
    if (tex_valid)
        gx2_gs_rebind_texture(tex_linear, tex_wrap);
}

/* Wait for the GPU; afterwards the vertex buffer and the texture may be rewritten. */
static void gpu_wait(void)
{
    GX2DrawDone();
    stats[3]++;
    vb_used = vb_flushed = 0;
    tex_pending_use = 0;
}

void gx2_gs_begin(int clear, uint32_t argb)
{
    if (!ready)
        return;
    gpu_wait();
    if (clear) {
        const float a = (float)(argb >> 24) / 128.0f;
        GX2ClearColor(&target, (float)((argb >> 16) & 0xffu) / 255.0f, (float)((argb >> 8) & 0xffu) / 255.0f,
                      (float)(argb & 0xffu) / 255.0f, a > 1.0f ? 1.0f : a);
    }
    bind_state();
    gx2_gs_set_blend(GX2_GS_BLEND_OFF, 0x80);
}

int gx2_gs_set_texture(const uint32_t *argb, unsigned w, unsigned h, int has_alpha, int linear, int wrap)
{
    return gx2_gs_set_texture_rect(argb, w, h, 0, 0, w, h, has_alpha, linear, wrap);
}

int gx2_gs_set_texture_rect(const uint32_t *argb, unsigned w, unsigned h, unsigned rx0, unsigned ry0, unsigned rx1,
                            unsigned ry1, int has_alpha, int linear, int wrap)
{
    if (!ready || !argb || !w || !h || w > tw_max || h > th_max || rx1 > w || ry1 > h || rx0 >= rx1 || ry0 >= ry1)
        return 0;
    /* A fresh ring slot per upload: the previous texture may still be in use by queued draws. Only the rectangle
     * rx0..rx1 x ry0..ry1 is valid; draws bound to this texture sample inside it. */
    /* Clamped textures get two extra rows below the image: a copy of the last row (keeps edge filtering as with
     * clamping) and an upload counter. Cemu re-checks texture memory sparsely and otherwise kept a stale texture
     * when a new one of the same size landed on a reused ring address (Tekken: a second Namco logo). The v
     * coordinates are scaled by h / (h + 2) in gx2_gs_draw. Repeating (wrap) textures cannot be padded this way. */
    const unsigned pad = (wrap || !tex_pad_rows) ? 0u : 2u;
    uint32_t *dst = tex_in_ring(&tex_view, w, h + pad, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8);
    if (!dst) {
        view_ok = 0;
        return 0;
    }
    view_ok = 1;
    const unsigned pitch = tex_view.surface.pitch;
    if (rx0 == 0 && rx1 == w)
        dcbz_range(dst + (size_t)ry0 * pitch, (size_t)(ry1 - ry0) * pitch * 4u);
    for (unsigned y = ry0; y < ry1; y++) {
        uint32_t *row = dst + (size_t)y * pitch;
        const uint32_t *src = argb + (size_t)y * w;
        for (unsigned x = rx0; x < rx1; x++) {
            const uint32_t c = src[x];
            /* GS texture alpha 0x80 = 1.0; the shader multiplies, so scale it to the 0..255 = 0..2 range halfway. */
            uint32_t a = has_alpha ? (c >> 24) * 2u : 0xffu;
            if (a > 0xffu) a = 0xffu;
            row[x] = (c << 8) | a;
        }
    }
    if (pad) {
        static uint32_t tex_nonce;
        tex_nonce++;
        memcpy(dst + (size_t)h * pitch, dst + (size_t)(h - 1u) * pitch, w * 4u);
        uint32_t *nr = dst + (size_t)(h + 1u) * pitch;
        for (unsigned x = 0; x < pitch; x++)
            nr[x] = tex_nonce;
    }
    tex_vscale = pad ? (float)h / (float)(h + pad) : 1.0f;
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, tex_view.surface.image, tex_view.surface.imageSize);
    GX2SetPixelTexture(&tex_view, sh_mod.pixelShader->samplerVars[0].location);
    GX2SetPixelSampler(&samplers[linear ? 1 : 0][wrap ? 1 : 0], sh_mod.pixelShader->samplerVars[0].location);
    tex_w_used = w;
    tex_h_used = h;
    tex_linear = linear;
    tex_wrap = wrap;
    tex_valid = 1;
    stats[2]++;
    return 1;
}

void gx2_gs_rebind_texture(int linear, int wrap)
{
    if (!ready || !tex_valid || !view_ok)
        return;
    GX2SetPixelTexture(&tex_view, sh_mod.pixelShader->samplerVars[0].location);
    GX2SetPixelSampler(&samplers[linear ? 1 : 0][wrap ? 1 : 0], sh_mod.pixelShader->samplerVars[0].location);
    tex_linear = linear;
    tex_wrap = wrap;
}

/* Frame buffer feedback without a CPU round trip: the texture is a rectangle of the GPU target. Copies target texels
 * (ox+rx0..ox+rx1, oy+ry0..oy+ry1) into a tw x th texture at (rx0, ry0) with a drawn quad (point sampling, blending
 * and depth off) and binds that texture. Without this a texture read from the target resolved it (GPU wait, CPU
 * readback of the whole target, texture decode, upload of the target again): the PS2 BIOS intro does that ~10 times
 * per second. alpha_const < 0: texel alpha copied (same 0x80 -> 0xff convention as texture uploads), otherwise the
 * texture alpha is set to alpha_const (0..1) (TCC=0 or PSMCT24 with TEXA.TA0). The copies live in their own memory
 * that only the GPU writes (Cemu then never mixes them with CPU-written textures at the same address). */
uint64_t gekko2_wiiu_fbtex_stats[2]; /* 0 copies, 1 copied texels */
int gx2_gs_texture_from_target(unsigned tw, unsigned th, unsigned ox, unsigned oy, unsigned rx0, unsigned ry0,
                               unsigned rx1, unsigned ry1, float alpha_const, float alpha_black, int linear, int wrap)
{
    /* alpha_black (only with the gs_fbcopy shader): alpha of texels whose RGB is 0 (PSMCT24 + TEXA.AEM). */
    if (alpha_const >= 0.0f && alpha_black != alpha_const && !sh_fbc.pixelShader)
        return 0;
    if (!ready || !tw || !th || tw > tw_max || th > th_max || rx1 > tw || ry1 > th || rx0 >= rx1 || ry0 >= ry1 ||
        ox + rx1 > cur_w || oy + ry1 > cur_h)
        return 0;
    if (!fb_ring) {
        fb_ring = MEMAllocFromDefaultHeapEx(FB_RING_BYTES, 0x1000);
        if (!fb_ring)
            return 0;
        fb_ring_off = 0;
    }
    static GX2ColorBuffer cb;
    memset(&cb, 0, sizeof cb);
    cb.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    cb.surface.width = tw;
    cb.surface.height = th;
    cb.surface.depth = 1;
    cb.surface.mipLevels = 1;
    cb.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    cb.surface.aa = GX2_AA_MODE1X;
    cb.surface.use = GX2_SURFACE_USE_TEXTURE | GX2_SURFACE_USE_COLOR_BUFFER;
    cb.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    cb.viewNumSlices = 1;
    GX2CalcSurfaceSizeAndAlignment(&cb.surface);
    uint32_t align = cb.surface.alignment < 256u ? 256u : cb.surface.alignment;
    uint32_t off = (fb_ring_off + align - 1u) & ~(align - 1u);
    if (cb.surface.imageSize > FB_RING_BYTES)
        return 0;
    if (off + cb.surface.imageSize > FB_RING_BYTES) {
        gpu_wait(); /* earlier copies may still be sampled by queued draws */
        bind_state();
        off = 0;
    }
    fb_ring_off = off + cb.surface.imageSize;
    cb.surface.image = fb_ring + off;
    GX2InitColorBufferRegs(&cb);
    if (vb_used + 4u > VB_MAX) {
        stats[4]++;
        gpu_wait();
        bind_state();
    }
    /* Source: the target memory as a texture. */
    static GX2Texture src;
    memset(&src, 0, sizeof src);
    src.surface = target.surface;
    src.surface.use = GX2_SURFACE_USE_TEXTURE;
    src.viewNumSlices = 1;
    src.compMap = 0x00010203;
    GX2InitTextureRegs(&src);
    /* Target rendering done before the copy reads it. */
    GX2Invalidate(GX2_INVALIDATE_MODE_COLOR_BUFFER, target.surface.image, target.surface.imageSize);
    GX2Invalidate(GX2_INVALIDATE_MODE_TEXTURE, target.surface.image, target.surface.imageSize);
    const int fbc = sh_fbc.pixelShader != NULL;
    if (alpha_const >= 0.0f && !fbc) {
        GX2ClearColor(&cb, 0.0f, 0.0f, 0.0f, alpha_const > 1.0f ? 1.0f : alpha_const);
        GX2SetContextState(ctx);
    }
    WHBGfxShaderGroup *g = fbc ? &sh_fbc : &sh_mod;
    state_forget();
    GX2SetColorBuffer(&cb, GX2_RENDER_TARGET_0);
    GX2SetViewport(0.0f, 0.0f, (float)tw, (float)th, 0.0f, 1.0f);
    GX2SetScissor(rx0, ry0, rx1 - rx0, ry1 - ry0);
    GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
    GX2SetColorControl(GX2_LOGIC_OP_COPY, 0, FALSE, TRUE);
    cur_blend = -1;
    GX2SetTargetChannelMasks((alpha_const >= 0.0f && !fbc) ? GX2_CHANNEL_MASK_RGB : GX2_CHANNEL_MASK_RGBA,
                             (GX2ChannelMask)0, (GX2ChannelMask)0, (GX2ChannelMask)0, (GX2ChannelMask)0,
                             (GX2ChannelMask)0, (GX2ChannelMask)0, (GX2ChannelMask)0);
    GX2SetFetchShader(&g->fetchShader);
    GX2SetVertexShader(g->vertexShader);
    GX2SetPixelShader(g->pixelShader);
    GX2SetPixelTexture(&src, g->pixelShader->samplerVars[0].location);
    GX2SetPixelSampler(&samplers[0][0], g->pixelShader->samplerVars[0].location);
    float col[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    if (fbc) {
        col[0] = alpha_const >= 0.0f ? 0.0f : 1.0f;
        col[1] = alpha_const >= 0.0f ? alpha_const : 0.0f;
        col[2] = alpha_const >= 0.0f ? alpha_black : 0.0f;
        col[3] = 0.0f;
    }
    const float W = (float)target.surface.width, H = (float)target.surface.height;
    const float u0 = (float)ox / W, u1 = (float)(ox + tw) / W, v0 = (float)oy / H, v1 = (float)(oy + th) / H;
    gx2_gs_vertex *q = vb + vb_used;
    const float px[4] = { -1.0f, 1.0f, 1.0f, -1.0f }, py[4] = { 1.0f, 1.0f, -1.0f, -1.0f };
    const float qu[4] = { u0, u1, u1, u0 }, qv[4] = { v0, v0, v1, v1 };
    for (unsigned i = 0; i < 4; i++) {
        q[i].pos[0] = px[i]; q[i].pos[1] = py[i]; q[i].pos[2] = 0.0f; q[i].pos[3] = 1.0f;
        q[i].col[0] = col[0]; q[i].col[1] = col[1]; q[i].col[2] = col[2]; q[i].col[3] = col[3];
        q[i].uv[0] = qu[i]; q[i].uv[1] = qv[i]; q[i].uv[2] = 0.0f; q[i].uv[3] = 1.0f;
    }
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, q, 4u * sizeof *q);
    GX2SetAttribBuffer(0, 4u * sizeof *q, sizeof *q, q);
    GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
    vb_used += 4u;
    GX2SetTargetChannelMasks(GX2_CHANNEL_MASK_RGBA, (GX2ChannelMask)0, (GX2ChannelMask)0, (GX2ChannelMask)0,
                             (GX2ChannelMask)0, (GX2ChannelMask)0, (GX2ChannelMask)0, (GX2ChannelMask)0);
    /* The copy is done before draws sample it. */
    GX2Invalidate(GX2_INVALIDATE_MODE_COLOR_BUFFER, cb.surface.image, cb.surface.imageSize);
    GX2Invalidate(GX2_INVALIDATE_MODE_TEXTURE, cb.surface.image, cb.surface.imageSize);
    memset(&tex_view, 0, sizeof tex_view);
    tex_view.surface = cb.surface;
    tex_view.surface.use = GX2_SURFACE_USE_TEXTURE;
    tex_view.viewNumSlices = 1;
    tex_view.compMap = 0x00010203;
    GX2InitTextureRegs(&tex_view);
    view_ok = 1;
    tex_vscale = 1.0f;
    tex_w_used = tw;
    tex_h_used = th;
    tex_linear = linear;
    tex_wrap = wrap;
    tex_valid = 1;
    tex_pending_use = 1;
    bind_state(); /* target, viewport, depth state, texture (tex_view) with this filter/wrap */
    gekko2_wiiu_fbtex_stats[0]++;
    gekko2_wiiu_fbtex_stats[1] += (uint64_t)(rx1 - rx0) * (ry1 - ry0);
    return 1;
}

void gx2_gs_mark_texture_used(void) { tex_pending_use = 1; }
uint32_t gx2_gs_texture_epoch(void) { return (uint32_t)ring_wraps; }

uint32_t *gx2_gs_target_cpu(unsigned w, unsigned h, unsigned *pitch)
{
    if (!ready || !w || !h || w > target.surface.width || h > target.surface.height)
        return NULL;
    gpu_wait();
    cur_w = w;
    cur_h = h;
    if (pitch)
        *pitch = cstage_pitch;
    return cstage;
}

void gx2_gs_target_ready(void)
{
    gx2_gs_target_ready_rows(0, cur_h);
}

void gx2_gs_target_ready_rows(unsigned y0, unsigned y1)
{
    if (!ready)
        return;
    if (y1 > cur_h) y1 = cur_h;
    bind_state();
    gx2_gs_set_blend(GX2_GS_BLEND_OFF, 0x80);
    if (y1 <= y0)
        return;
    if (gekko2_wiiu_upload_rle) {
        /* Upload without a texture: every run of equal pixels in a row becomes one flat-coloured quad. Default on Cemu:
         * there the texture path below sometimes drew an older upload of the same size from the same ring address
         * (measured with frame dumps: Tekken's Namco logo appeared a second time 88 rows lower on every other field;
         * with this path 0 of 106 frames, same speed). */
        GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
        state_forget();
        GX2SetFetchShader(&sh_color.fetchShader);
        GX2SetVertexShader(sh_color.vertexShader);
        GX2SetPixelShader(sh_color.pixelShader);
        const unsigned maxq = 2048u;
        for (unsigned y = y0; y < y1; y++) {
            const uint32_t *row = cstage + (size_t)y * cstage_pitch;
            unsigned x = 0;
            while (x < cur_w) {
                if (vb_used + maxq * 4u > VB_MAX) {
                    gpu_wait();
                    GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
                }
                gx2_gs_vertex *dst = vb + vb_used;
                unsigned nq = 0;
                while (x < cur_w && nq < maxq) {
                    const uint32_t c = row[x];
                    unsigned e = x + 1;
                    while (e < cur_w && row[e] == c) e++;
                    const float fx0 = (float)x / (float)cur_w * 2.0f - 1.0f, fx1 = (float)e / (float)cur_w * 2.0f - 1.0f;
                    const float fy0 = 1.0f - 2.0f * (float)y / (float)cur_h, fy1 = 1.0f - 2.0f * (float)(y + 1) / (float)cur_h;
                    const float px[4] = { fx0, fx1, fx1, fx0 }, py[4] = { fy0, fy0, fy1, fy1 };
                    for (int i = 0; i < 4; i++) {
                        gx2_gs_vertex *v = &dst[nq * 4u + (unsigned)i];
                        v->pos[0] = px[i]; v->pos[1] = py[i]; v->pos[2] = 0.0f; v->pos[3] = 1.0f;
                        v->col[0] = (float)(c >> 24) / 255.0f; v->col[1] = (float)((c >> 16) & 0xffu) / 255.0f;
                        v->col[2] = (float)((c >> 8) & 0xffu) / 255.0f; v->col[3] = (float)(c & 0xffu) / 255.0f;
                        v->uv[0] = v->uv[1] = v->uv[2] = 0.0f; v->uv[3] = 1.0f;
                    }
                    nq++;
                    x = e;
                }
                GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, dst, nq * 4u * sizeof *dst);
                GX2SetAttribBuffer(0, nq * 4u * sizeof *dst, sizeof *dst, dst);
                GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, nq * 4u, 0, 1);
                vb_used += nq * 4u;
            }
        }
        state_forget();
        if (zready && (cur_zfunc >= 0 || cur_zwrite))
            GX2SetDepthOnlyControl(TRUE, cur_zwrite ? TRUE : FALSE,
                                   cur_zfunc >= 0 ? (GX2CompareFunction)cur_zfunc : GX2_COMPARE_FUNC_ALWAYS);
        return;
    }
    /* The CPU filled rows y0..y1 of the staging texture; the GPU copies them onto the target with a quad (no
     * blending, no depth). Writing the colour buffer memory directly is not seen by Cemu, whose render targets live
     * in host textures. */
    /* One extra row (never sampled) holds an upload counter: Cemu samples texture memory sparsely to detect
     * changes, and mostly black rows uploaded to a reused address looked unchanged (stale picture rows). */
    const unsigned rows = y1 - y0;
    uint32_t *img = tex_in_ring(&up_tex, cur_w, rows + 1u, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8);
    if (!img)
        return;
    dcbz_range(img, up_tex.surface.imageSize);
    for (unsigned y = y0; y < y1; y++)
        memcpy(img + (size_t)(y - y0) * up_tex.surface.pitch, cstage + (size_t)y * cstage_pitch, cur_w * 4u);
    {
        static uint32_t nonce;
        nonce++;
        uint32_t *nr = img + (size_t)rows * up_tex.surface.pitch;
        for (unsigned x = 0; x < up_tex.surface.pitch; x++)
            nr[x] = nonce;
    }
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, img, up_tex.surface.imageSize);
    if (vb_used + 4 > VB_MAX) {
        gpu_wait();
        bind_state();
        gx2_gs_set_blend(GX2_GS_BLEND_OFF, 0x80);
    }
    GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
    GX2SetPixelTexture(&up_tex, sh_mod.pixelShader->samplerVars[0].location);
    GX2SetPixelSampler(&samplers[0][0], sh_mod.pixelShader->samplerVars[0].location);
    gx2_gs_vertex q[4];
    const float ya = 1.0f - 2.0f * (float)y0 / (float)cur_h, yb = 1.0f - 2.0f * (float)y1 / (float)cur_h;
    const float ue = 1.0f, va = 0.0f, vb2 = (float)rows / (float)(rows + 1u);
    const float px[4] = { -1, 1, 1, -1 }, py[4] = { ya, ya, yb, yb }, u[4] = { 0, ue, ue, 0 }, v[4] = { va, va, vb2, vb2 };
    for (int i = 0; i < 4; i++) {
        q[i].pos[0] = px[i]; q[i].pos[1] = py[i]; q[i].pos[2] = 0.0f; q[i].pos[3] = 1.0f;
        q[i].col[0] = q[i].col[1] = q[i].col[2] = q[i].col[3] = 1.0f;
        q[i].uv[0] = u[i]; q[i].uv[1] = v[i]; q[i].uv[2] = 0.0f; q[i].uv[3] = 1.0f;
    }
    gx2_gs_vertex *dst = vb + vb_used;
    memcpy(dst, q, sizeof q);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, dst, sizeof q);
    state_forget();
    GX2SetFetchShader(&sh_mod.fetchShader);
    GX2SetVertexShader(sh_mod.vertexShader);
    GX2SetPixelShader(sh_mod.pixelShader);
    GX2SetAttribBuffer(0, sizeof q, sizeof q[0], dst);
    GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
    vb_used += 4;
    /* The upload must finish before the staging rows are rewritten (target_cpu waits for the GPU). Restore the
     * depth state and the GS texture for the following draws. */
    if (zready && (cur_zfunc >= 0 || cur_zwrite))
        GX2SetDepthOnlyControl(TRUE, cur_zwrite ? TRUE : FALSE,
                               cur_zfunc >= 0 ? (GX2CompareFunction)cur_zfunc : GX2_COMPARE_FUNC_ALWAYS);
    if (tex_valid)
        gx2_gs_rebind_texture(tex_linear, tex_wrap);
}

void gx2_gs_set_scissor(unsigned x, unsigned y, unsigned w, unsigned h)
{
    if (!ready)
        return;
    if (ctx_lost) /* the presenter used the GPU in between: restore our context, viewport and texture */
        bind_state();
    if (x >= cur_w || y >= cur_h) { x = y = 0; w = h = 0; }
    if (x + w > cur_w) w = cur_w - x;
    if (y + h > cur_h) h = cur_h - y;
    if (st_sc[0] == x && st_sc[1] == y && st_sc[2] == w && st_sc[3] == h)
        return;
    st_sc[0] = x; st_sc[1] = y; st_sc[2] = w; st_sc[3] = h;
    GX2SetScissor(x, y, w, h);
}

void gx2_gs_size(unsigned *w, unsigned *h)
{
    if (w) *w = cur_w;
    if (h) *h = cur_h;
}

void gx2_gs_set_blend(int mode, uint32_t fix)
{
    if (!ready || mode == cur_blend)
        return;
    cur_blend = mode;
    if (mode == GX2_GS_BLEND_OFF) {
        GX2SetColorControl(GX2_LOGIC_OP_COPY, 0, FALSE, TRUE);
        return;
    }
    GX2BlendMode s = GX2_BLEND_MODE_SRC_ALPHA, d = GX2_BLEND_MODE_ONE;
    GX2BlendCombineMode op = GX2_BLEND_COMBINE_MODE_ADD;
    switch (mode) {
    case GX2_GS_BLEND_MIX_AS: s = GX2_BLEND_MODE_SRC_ALPHA; d = GX2_BLEND_MODE_INV_SRC_ALPHA; break;
    case GX2_GS_BLEND_ADD_AS: s = GX2_BLEND_MODE_SRC_ALPHA; d = GX2_BLEND_MODE_ONE; break;
    case GX2_GS_BLEND_ADD_FIX: s = GX2_BLEND_MODE_BLEND_FACTOR; d = GX2_BLEND_MODE_ONE; break;
    case GX2_GS_BLEND_SUB_AS: s = GX2_BLEND_MODE_SRC_ALPHA; d = GX2_BLEND_MODE_ONE; op = GX2_BLEND_COMBINE_MODE_REV_SUB; break;
    case GX2_GS_BLEND_MIX_FIX: s = GX2_BLEND_MODE_BLEND_FACTOR; d = GX2_BLEND_MODE_INV_BLEND_FACTOR; break;
    case GX2_GS_BLEND_ADD: s = GX2_BLEND_MODE_ONE; d = GX2_BLEND_MODE_ONE; break;
    case GX2_GS_BLEND_KEEP: s = GX2_BLEND_MODE_ZERO; d = GX2_BLEND_MODE_ONE; break;
    case GX2_GS_BLEND_SUB_FIX: s = GX2_BLEND_MODE_BLEND_FACTOR; d = GX2_BLEND_MODE_ONE; op = GX2_BLEND_COMBINE_MODE_REV_SUB; break;
    case GX2_GS_BLEND_MIX_AD: s = GX2_BLEND_MODE_DST_ALPHA; d = GX2_BLEND_MODE_INV_DST_ALPHA; break;
    case GX2_GS_BLEND_ADD_AD: s = GX2_BLEND_MODE_DST_ALPHA; d = GX2_BLEND_MODE_ONE; break;
    default: break;
    }
    float f = (float)fix / 128.0f;
    if (f > 1.0f) f = 1.0f;
    GX2SetBlendConstantColor(f, f, f, f);
    /* GS writes the source alpha unblended into the frame buffer. */
    GX2SetBlendControl(GX2_RENDER_TARGET_0, s, d, op, TRUE, GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_ZERO,
                       GX2_BLEND_COMBINE_MODE_ADD);
    GX2SetColorControl(GX2_LOGIC_OP_COPY, 1, FALSE, TRUE);
}

int gx2_gs_draw(const gx2_gs_vertex *v, unsigned count, int prim, int textured)
{
    if (!ready || !v || !count || count > VB_MAX)
        return 0;
    if (vb_used + count > VB_MAX) {
        stats[4]++;
        gpu_wait();
        bind_state();
    }
    gx2_gs_vertex *dst = vb + vb_used;
    memcpy(dst, v, count * sizeof *v);
    if (textured && tex_vscale != 1.0f)
        for (unsigned i = 0; i < count; i++)
            dst[i].uv[1] *= tex_vscale;
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, dst, count * sizeof *v);
    WHBGfxShaderGroup *g = textured ? &sh_mod : &sh_color;
    if (st_shader != g) {
        st_shader = g;
        GX2SetFetchShader(&g->fetchShader);
        GX2SetVertexShader(g->vertexShader);
        GX2SetPixelShader(g->pixelShader);
    }
    GX2SetAttribBuffer(0, count * sizeof *v, sizeof *v, dst);
    const GX2PrimitiveMode mode = prim == GX2_GS_STRIP ? GX2_PRIMITIVE_MODE_TRIANGLE_STRIP
                                  : prim == GX2_GS_FAN ? GX2_PRIMITIVE_MODE_TRIANGLE_FAN
                                  : prim == GX2_GS_QUADS ? GX2_PRIMITIVE_MODE_QUADS : GX2_PRIMITIVE_MODE_TRIANGLES;
    GX2DrawEx(mode, count, 0, 1);
    vb_used += count;
    if (textured)
        tex_pending_use = 1;
    stats[0]++;
    stats[1] += count;
    return 1;
}

const uint32_t *gx2_gs_end(unsigned *pitch)
{
    return gx2_gs_end_rows(0, target.surface.height, pitch);
}

const uint32_t *gx2_gs_end_rows(unsigned y0, unsigned y1, unsigned *pitch)
{
    if (!ready)
        return NULL;
    GX2Flush();
    gpu_wait();
    if (y1 > target.surface.height) y1 = target.surface.height;
    /* GPU writes went to memory; drop stale CPU cache lines of the rows we read. */
    if (y1 > y0)
        DCInvalidateRange((uint8_t *)target.surface.image + (size_t)y0 * target.surface.pitch * 4u,
                          (uint32_t)((y1 - y0) * target.surface.pitch * 4u));
    if (gekko2_wiiu_rb_check && y1 > y0) {
        /* Diagnostics (rb_check.txt): read the rows, sync the GPU again and compare. A difference means the first
         * read saw memory that the GPU readback had not finished writing yet. */
        const uint32_t *t = (const uint32_t *)target.surface.image;
        uint64_t h1 = 0, h2 = 0;
        for (unsigned y = y0; y < y1; y++)
            for (unsigned x = 0; x < cur_w; x++)
                h1 = h1 * 31u + t[(size_t)y * target.surface.pitch + x];
        GX2DrawDone();
        DCInvalidateRange((uint8_t *)target.surface.image + (size_t)y0 * target.surface.pitch * 4u,
                          (uint32_t)((y1 - y0) * target.surface.pitch * 4u));
        for (unsigned y = y0; y < y1; y++)
            for (unsigned x = 0; x < cur_w; x++)
                h2 = h2 * 31u + t[(size_t)y * target.surface.pitch + x];
        gekko2_wiiu_rb_stats[0]++;
        if (h1 != h2)
            gekko2_wiiu_rb_stats[1]++;
    }
    if (pitch)
        *pitch = target.surface.pitch;
    return (const uint32_t *)target.surface.image;
}

/* ---------------------------------------------------------------------------------------------- depth ---- */

const char *gx2_gs_depth_status(void) { return z_status; }
int gx2_gs_depth_ready(void) { return zready; }

void gx2_gs_set_depth(int func, int write)
{
    if (!ready || !zready)
        return;
    if (func == cur_zfunc && (write != 0) == (cur_zwrite != 0))
        return;
    cur_zfunc = func;
    cur_zwrite = write != 0;
    if (cur_zfunc >= 0 || cur_zwrite)
        GX2SetDepthOnlyControl(TRUE, cur_zwrite ? TRUE : FALSE,
                               cur_zfunc >= 0 ? (GX2CompareFunction)cur_zfunc : GX2_COMPARE_FUNC_ALWAYS);
    else
        GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
}

float *gx2_gs_depth_cpu(unsigned *pitch)
{
    if (!ready || !ztex.surface.image)
        return NULL;
    gpu_wait();
    if (pitch)
        *pitch = ztex.surface.pitch;
    return (float *)ztex.surface.image;
}

static uint32_t swap32(uint32_t v) { return (v >> 24) | ((v >> 8) & 0xff00u) | ((v << 8) & 0xff0000u) | (v << 24); }

/* Full-target pass: depth = sampled float, colour writes off. The caller filled gx2_gs_depth_cpu() for cur_w x cur_h. */
static void depth_upload_pass_rows(int swap, unsigned y0, unsigned y1);
static void depth_upload_pass(int swap) { depth_upload_pass_rows(swap, 0, cur_h); }
static void depth_upload_pass_rows(int swap, unsigned y0, unsigned y1)
{
    if (y1 > cur_h) y1 = cur_h;
    if (y1 <= y0)
        return;
    static GX2Texture zup;
    const unsigned zrows = y1 - y0;
    uint32_t *img = tex_in_ring(&zup, cur_w, zrows + 1u, GX2_SURFACE_FORMAT_FLOAT_R32); /* + counter row, as above */
    if (!img)
        return;
    dcbz_range(img, zup.surface.imageSize);
    {
        static uint32_t znonce;
        znonce++;
        uint32_t *nr = img + (size_t)zrows * zup.surface.pitch;
        for (unsigned x = 0; x < zup.surface.pitch; x++)
            nr[x] = znonce;
    }
    const uint32_t *zs = (const uint32_t *)ztex.surface.image;
    for (unsigned y = y0; y < y1; y++) {
        uint32_t *d = img + (size_t)(y - y0) * zup.surface.pitch;
        const uint32_t *r = zs + (size_t)y * ztex.surface.pitch;
        if (swap)
            for (unsigned x = 0; x < cur_w; x++)
                d[x] = swap32(r[x]);
        else
            memcpy(d, r, cur_w * 4u);
    }
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, img, zup.surface.imageSize);
    bind_state();
    GX2SetColorControl(GX2_LOGIC_OP_COPY, 0, FALSE, FALSE);
    GX2SetDepthOnlyControl(TRUE, TRUE, GX2_COMPARE_FUNC_ALWAYS);
    GX2SetPixelTexture(&zup, sh_zload.pixelShader->samplerVars[0].location);
    GX2SetPixelSampler(&samplers[0][0], sh_zload.pixelShader->samplerVars[0].location);
    gx2_gs_vertex q[4];
    const float ya = 1.0f - 2.0f * (float)y0 / (float)cur_h, yb = 1.0f - 2.0f * (float)y1 / (float)cur_h;
    const float ue = 1.0f, va = 0.0f, vb2 = (float)zrows / (float)(zrows + 1u);
    const float px[4] = { -1, 1, 1, -1 }, py[4] = { ya, ya, yb, yb }, u[4] = { 0, ue, ue, 0 }, v[4] = { va, va, vb2, vb2 };
    for (int i = 0; i < 4; i++) {
        q[i].pos[0] = px[i]; q[i].pos[1] = py[i]; q[i].pos[2] = 0.0f; q[i].pos[3] = 1.0f;
        q[i].col[0] = q[i].col[1] = q[i].col[2] = q[i].col[3] = 1.0f;
        q[i].uv[0] = u[i]; q[i].uv[1] = v[i]; q[i].uv[2] = 0.0f; q[i].uv[3] = 1.0f;
    }
    gx2_gs_vertex *dst = vb + vb_used;
    memcpy(dst, q, sizeof q);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, dst, sizeof q);
    state_forget();
    GX2SetFetchShader(&sh_zload.fetchShader);
    GX2SetVertexShader(sh_zload.vertexShader);
    GX2SetPixelShader(sh_zload.pixelShader);
    GX2SetAttribBuffer(0, sizeof q, sizeof q[0], dst);
    GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
    vb_used += 4;
    /* Restore the GS state; the texture slot binding is restored by bind_state. */
    gpu_wait();
    bind_state();
}

void gx2_gs_depth_upload(void)
{
    if (!ready || !zready)
        return;
    depth_upload_pass(z_upload_swap);
}

void gx2_gs_depth_upload_rows(unsigned y0, unsigned y1)
{
    if (!ready || !zready)
        return;
    depth_upload_pass_rows(z_upload_swap, y0, y1);
}

void gx2_gs_depth_clear(float d)
{
    if (!ready || !zready)
        return;
    gpu_wait();
    GX2ClearDepthStencilEx(&zbuf, d, 0, GX2_CLEAR_FLAGS_BOTH);
    bind_state();
}

/* Readback B (used when A returns nothing, as on Cemu): convert the depth buffer to a texture and draw it with
 * the modulate shader (colour 1.0) into a linear FLOAT_R32 colour buffer; the CPU reads floats 0..1. */
static const uint32_t *depth_read_shader_rows(unsigned y0, unsigned y1, unsigned *pitch)
{
    GX2Flush();
    gpu_wait();
    GX2ConvertDepthBufferToTextureSurface(&zbuf, &zconv, 0, 0);
    GX2SetContextState(ctx);
    GX2SetColorBuffer(&zrb, GX2_RENDER_TARGET_0);
    GX2SetViewport(0.0f, 0.0f, (float)cur_w, (float)cur_h, 0.0f, 1.0f);
    GX2SetScissor(0, 0, cur_w, cur_h);
    GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
    GX2SetColorControl(GX2_LOGIC_OP_COPY, 0, FALSE, TRUE);
    GX2SetPixelTexture(&zconv_tex, sh_mod.pixelShader->samplerVars[0].location);
    GX2SetPixelSampler(&samplers[0][0], sh_mod.pixelShader->samplerVars[0].location);
    gx2_gs_vertex q[4];
    const float ya = 1.0f - 2.0f * (float)y0 / (float)cur_h, yb = 1.0f - 2.0f * (float)y1 / (float)cur_h;
    const float ue = (float)cur_w / (float)zconv.width;
    const float va = (float)y0 / (float)zconv.height, vb2 = (float)y1 / (float)zconv.height;
    const float px[4] = { -1, 1, 1, -1 }, py[4] = { ya, ya, yb, yb }, u[4] = { 0, ue, ue, 0 }, v[4] = { va, va, vb2, vb2 };
    for (int i = 0; i < 4; i++) {
        q[i].pos[0] = px[i]; q[i].pos[1] = py[i]; q[i].pos[2] = 0.0f; q[i].pos[3] = 1.0f;
        q[i].col[0] = q[i].col[1] = q[i].col[2] = q[i].col[3] = 1.0f;
        q[i].uv[0] = u[i]; q[i].uv[1] = v[i]; q[i].uv[2] = 0.0f; q[i].uv[3] = 1.0f;
    }
    gx2_gs_vertex *dst = vb + vb_used;
    memcpy(dst, q, sizeof q);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, dst, sizeof q);
    state_forget();
    GX2SetFetchShader(&sh_mod.fetchShader);
    GX2SetVertexShader(sh_mod.vertexShader);
    GX2SetPixelShader(sh_mod.pixelShader);
    GX2SetAttribBuffer(0, sizeof q, sizeof q[0], dst);
    GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
    vb_used += 4;
    GX2Flush();
    gpu_wait();
    if (y1 > zrb.surface.height) y1 = zrb.surface.height;
    if (y1 > y0)
        DCInvalidateRange((uint8_t *)zrb.surface.image + (size_t)y0 * zrb.surface.pitch * 4u,
                          (uint32_t)((y1 - y0) * zrb.surface.pitch * 4u));
    ctx_lost = 1; /* colour buffer, texture and shaders changed: the next GS draw restores everything */
    if (pitch)
        *pitch = zrb.surface.pitch;
    return (const uint32_t *)zrb.surface.image;
}
static const uint32_t *depth_read_raw_rows(unsigned y0, unsigned y1, unsigned *pitch);
static const uint32_t *depth_read_raw(unsigned *pitch) { return depth_read_raw_rows(0, zlin.height, pitch); }
static const uint32_t *depth_read_raw_rows(unsigned y0, unsigned y1, unsigned *pitch)
{
    if (z_read_shader)
        return depth_read_shader_rows(y0, y1, pitch);
    GX2Flush();
    gpu_wait();
    GX2ConvertDepthBufferToTextureSurface(&zbuf, &zconv, 0, 0);
    GX2CopySurface(&zconv, 0, 0, &zlin, 0, 0);
    GX2Flush();
    gpu_wait();
    if (y1 > zlin.height) y1 = zlin.height;
    if (y1 > y0)
        DCInvalidateRange((uint8_t *)zlin.image + (size_t)y0 * zlin.pitch * 4u, (uint32_t)((y1 - y0) * zlin.pitch * 4u));
    ctx_lost = 1;
    if (pitch)
        *pitch = zlin.pitch;
    return (const uint32_t *)zlin.image;
}

uint32_t gx2_gs_depth_word(uint32_t w)
{
    if (z_read_shader) { /* FLOAT_R32 0..1 */
        union { uint32_t u; float f; } c;
        c.u = z_read_swap ? __builtin_bswap32(w) : w;
        if (!(c.f > 0.0f)) return 0;
        if (c.f >= 1.0f) return 0xffffffu;
        return (uint32_t)(c.f * 16777215.0f + 0.5f);
    }
    switch (z_word_mode) {
    case 1: return swap32(w) & 0xffffffu;
    case 2: return w >> 8;
    case 3: return swap32(w) >> 8;
    default: return w & 0xffffffu;
    }
}

const uint32_t *gx2_gs_depth_read(unsigned *pitch)
{
    if (!ready || !zready)
        return NULL;
    return depth_read_raw(pitch);
}

const uint32_t *gx2_gs_depth_read_rows(unsigned y0, unsigned y1, unsigned *pitch)
{
    if (!ready || !zready)
        return NULL;
    return depth_read_raw_rows(y0, y1, pitch);
}

static int near24(uint32_t a, uint32_t b) { return (a > b ? a - b : b - a) <= 0x400u; }

int gx2_gs_init_depth(const void *gsh_zload)
{
    if (!ready || zready || !gsh_zload)
        return 0;
    if (!load_group(&sh_zload, gsh_zload)) {
        snprintf(z_status, sizeof z_status, "depth: zload shader rejected");
        return 0;
    }
    memset(&zbuf, 0, sizeof zbuf);
    zbuf.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    zbuf.surface.width = target.surface.width;
    zbuf.surface.height = target.surface.height;
    zbuf.surface.depth = 1;
    zbuf.surface.mipLevels = 1;
    zbuf.surface.format = (GX2SurfaceFormat)0x011; /* D24_S8 UNORM */
    zbuf.surface.aa = GX2_AA_MODE1X;
    zbuf.surface.use = GX2_SURFACE_USE_DEPTH_BUFFER;
    zbuf.surface.tileMode = GX2_TILE_MODE_DEFAULT;
    zbuf.viewNumSlices = 1;
    zbuf.depthClear = 1.0f;
    GX2CalcSurfaceSizeAndAlignment(&zbuf.surface);
    GX2InitDepthBufferRegs(&zbuf);
    zbuf.surface.image = MEMAllocFromDefaultHeapEx(zbuf.surface.imageSize, zbuf.surface.alignment);
    zconv = zbuf.surface;
    zconv.use = GX2_SURFACE_USE_TEXTURE;
    zconv.image = MEMAllocFromDefaultHeapEx(zconv.imageSize, zconv.alignment);
    memset(&zlin, 0, sizeof zlin);
    zlin.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    zlin.width = zbuf.surface.width;
    zlin.height = zbuf.surface.height;
    zlin.depth = 1;
    zlin.mipLevels = 1;
    zlin.format = GX2_SURFACE_FORMAT_UNORM_R24_X8;
    zlin.use = GX2_SURFACE_USE_TEXTURE;
    zlin.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    GX2CalcSurfaceSizeAndAlignment(&zlin);
    zlin.image = MEMAllocFromDefaultHeapEx(zlin.imageSize, zlin.alignment);
    memset(&ztex, 0, sizeof ztex);
    ztex.surface.width = zbuf.surface.width;
    ztex.surface.height = zbuf.surface.height;
    ztex.surface.depth = 1;
    ztex.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    ztex.surface.format = GX2_SURFACE_FORMAT_FLOAT_R32;
    ztex.surface.use = GX2_SURFACE_USE_TEXTURE;
    ztex.surface.mipLevels = 1;
    ztex.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    ztex.viewNumSlices = 1;
    ztex.compMap = 0x00010203;
    GX2CalcSurfaceSizeAndAlignment(&ztex.surface);
    GX2InitTextureRegs(&ztex);
    ztex.surface.image = MEMAllocFromDefaultHeapEx(ztex.surface.imageSize, ztex.surface.alignment);
    memset(&zconv_tex, 0, sizeof zconv_tex);
    zconv_tex.surface = zconv;
    zconv_tex.viewNumSlices = 1;
    zconv_tex.compMap = 0x00010203;
    GX2InitTextureRegs(&zconv_tex);
    memset(&zrb, 0, sizeof zrb);
    zrb.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    zrb.surface.width = zbuf.surface.width;
    zrb.surface.height = zbuf.surface.height;
    zrb.surface.depth = 1;
    zrb.surface.mipLevels = 1;
    zrb.surface.format = GX2_SURFACE_FORMAT_FLOAT_R32;
    zrb.surface.aa = GX2_AA_MODE1X;
    zrb.surface.use = GX2_SURFACE_USE_COLOR_BUFFER;
    zrb.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    zrb.viewNumSlices = 1;
    GX2CalcSurfaceSizeAndAlignment(&zrb.surface);
    GX2InitColorBufferRegs(&zrb);
    zrb.surface.image = MEMAllocFromDefaultHeapEx(zrb.surface.imageSize, zrb.surface.alignment);
    if (!zbuf.surface.image || !zconv.image || !zlin.image || !ztex.surface.image || !zrb.surface.image) {
        snprintf(z_status, sizeof z_status, "depth: out of memory");
        return 0;
    }
    zready = 1;
    const unsigned keep_w = cur_w, keep_h = cur_h;
    cur_w = 64;
    cur_h = 64;
    /* 1. readback word layout: clear to 0.25 and find where 0x3fffff appears */
    gx2_gs_depth_clear(0.25f);
    unsigned pitch = 0;
    uint32_t w = depth_read_raw(&pitch)[0];
    const uint32_t want = 0x3fffffu;
    z_word_mode = -1;
    for (int m = 0; m < 4; m++) {
        z_word_mode = m;
        if (near24(gx2_gs_depth_word(w), want))
            break;
        z_word_mode = -1;
    }
    if (z_word_mode < 0) {
        /* Readback A gave nothing usable (Cemu returns zeros): try readback B through a shader. */
        z_read_shader = 1;
        gx2_gs_depth_clear(0.25f);
        const uint32_t w2 = depth_read_raw(&pitch)[0];
        for (int sw = 0; sw < 2 && z_word_mode < 0; sw++) {
            z_read_swap = sw;
            if (near24(gx2_gs_depth_word(w2), want))
                z_word_mode = 4;
        }
        if (z_word_mode < 0)
            z_read_shader = z_read_swap = 0;
        if (z_word_mode < 0)
            snprintf(z_status, sizeof z_status, "depth: readback failed (A: word %08x, B: word %08x after clear 0.25)",
                     (unsigned)w, (unsigned)w2);
    }
    if (z_word_mode < 0) {
        zready = 0;
        cur_w = keep_w;
        cur_h = keep_h;
        return 0;
    }
    /* 2. upload float layout: write 0.75 and check the readback, first native, then byte-swapped */
    const uint32_t want2 = 0xbfffffu;
    uint32_t z_swap0 = 0;
    z_upload_swap = -1;
    for (int sw = 0; sw < 2; sw++) {
        float *f = gx2_gs_depth_cpu(&pitch);
        for (unsigned y = 0; y < cur_h; y++)
            for (unsigned x = 0; x < cur_w; x++)
                f[y * pitch + x] = 0.75f;
        depth_upload_pass(sw);
        const uint32_t got = gx2_gs_depth_word(depth_read_raw(&pitch)[pitch * 10 + 10]);
        if (sw == 0)
            z_swap0 = got;
        if (near24(got, want2)) {
            z_upload_swap = sw;
            break;
        }
        snprintf(z_status, sizeof z_status, "depth: upload swap=%d read %06x, want %06x", sw, (unsigned)got, (unsigned)want2);
    }
    if (z_upload_swap < 0) {
        /* Diagnostics for the log: does the readback follow a new clear, and do ordinary depth writes from
         * geometry (vertex z) work where the shader depth export does not? */
        char first[96];
        snprintf(first, sizeof first, "%s", z_status);
        gx2_gs_depth_clear(0.5f);
        const uint32_t c5 = gx2_gs_depth_word(depth_read_raw(&pitch)[pitch * 10 + 10]);
        gx2_gs_vertex t[3];
        memset(t, 0, sizeof t);
        const float tx[3] = { -1.0f, 3.0f, -1.0f }, ty[3] = { 1.0f, 1.0f, -3.0f };
        for (int i = 0; i < 3; i++) {
            t[i].pos[0] = tx[i]; t[i].pos[1] = ty[i]; t[i].pos[2] = 0.75f; t[i].pos[3] = 1.0f;
            t[i].col[0] = t[i].col[1] = t[i].col[2] = t[i].col[3] = 1.0f;
        }
        bind_state();
        cur_zfunc = -2;
        gx2_gs_set_depth(GX2_COMPARE_FUNC_ALWAYS, 1);
        gx2_gs_draw(t, 3, GX2_GS_TRIANGLES, 0);
        gx2_gs_set_depth(-1, 0);
        const uint32_t g75 = gx2_gs_depth_word(depth_read_raw(&pitch)[pitch * 10 + 10]);
        snprintf(z_status, sizeof z_status, "%s; swap0 %06x; clear0.5 -> %06x; vertex z0.75 -> %06x; readback %s",
                 first, (unsigned)z_swap0, (unsigned)c5, (unsigned)g75, z_read_shader ? "B" : "A");
        cur_w = keep_w;
        cur_h = keep_h;
        zready = 0;
        return 0;
    }
    cur_w = keep_w;
    cur_h = keep_h;
    snprintf(z_status, sizeof z_status, "depth ready (readback %s, word mode %d, upload swap %d, tile %d)",
             z_read_shader ? "B shader" : "A copy", z_word_mode, z_upload_swap,
             (int)zbuf.surface.tileMode);
    ctx_lost = 1;
    return 1;
}
