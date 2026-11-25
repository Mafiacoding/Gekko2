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

static WHBGfxShaderGroup sh_color, sh_mod;
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
static int view_ok;
/* Colour uploads go through a staging texture drawn onto the target (see gx2_gs_target_ready_rows). */
static GX2Texture utex;
/* Cemu checks a texture for CPU changes at most once per frame unless an invalidation starts at the texture's
 * base address, and it samples the memory sparsely. Every CPU-filled texture therefore gets 64 spare texels per
 * row (never sampled) that receive a new marker value with each upload, and invalidations start at the base. */
#define G2_PAD_TEXELS 64u
static uint32_t upload_gen;
static void pad_surface(GX2Surface *s)
{
    s->pitch = ((s->width + 63u) & ~63u) + G2_PAD_TEXELS;
    s->imageSize = s->pitch * s->height * 4u;
}
static void mark_rows(uint32_t *base, unsigned pitch, unsigned width, unsigned y0, unsigned y1)
{
    upload_gen++;
    for (unsigned y = y0; y < y1; y++) {
        uint32_t *row = base + (size_t)y * pitch;
        for (unsigned x = width; x < pitch; x++)
            row[x] = upload_gen;
    }
}
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

    memset(&tex, 0, sizeof tex);
    tex.surface.width = tw_max;
    tex.surface.height = th_max;
    tex.surface.depth = 1;
    tex.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    tex.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    tex.surface.use = GX2_SURFACE_USE_TEXTURE;
    tex.surface.mipLevels = 1;
    tex.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    tex.viewNumSlices = 1;
    tex.compMap = 0x00010203;
    GX2CalcSurfaceSizeAndAlignment(&tex.surface);
    pad_surface(&tex.surface);
    GX2InitTextureRegs(&tex);
    tex.surface.image = MEMAllocFromDefaultHeapEx(tex.surface.imageSize, tex.surface.alignment);
    if (!tex.surface.image)
        goto fail;
    memset(tex.surface.image, 0, tex.surface.imageSize);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, tex.surface.image, tex.surface.imageSize);

    memset(&utex, 0, sizeof utex);
    utex.surface.width = target.surface.width;
    utex.surface.height = target.surface.height;
    utex.surface.depth = 1;
    utex.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    utex.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    utex.surface.use = GX2_SURFACE_USE_TEXTURE;
    utex.surface.mipLevels = 1;
    utex.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    utex.viewNumSlices = 1;
    utex.compMap = 0x00010203;
    GX2CalcSurfaceSizeAndAlignment(&utex.surface);
    pad_surface(&utex.surface);
    GX2InitTextureRegs(&utex);
    utex.surface.image = MEMAllocFromDefaultHeapEx(utex.surface.imageSize, utex.surface.alignment);
    if (!utex.surface.image)
        goto fail;
    memset(utex.surface.image, 0, utex.surface.imageSize);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, utex.surface.image, utex.surface.imageSize);

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

void gx2_gs_shutdown(void)
{
    if (ready)
        GX2DrawDone();
    if (vb) MEMFreeToDefaultHeap(vb);
    if (tex.surface.image) MEMFreeToDefaultHeap(tex.surface.image);
    if (utex.surface.image) MEMFreeToDefaultHeap(utex.surface.image);
    memset(&utex, 0, sizeof utex);
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

/* The bound texture is a separate, persistent GX2Texture describing exactly w x h (its own pitch) over the slot's
 * memory. Its fields are never changed while it is bound: Cemu showed black textures when the old code bound a
 * struct whose width/height were reset to the slot size right after GX2SetPixelTexture. */
static int prepare_view(unsigned w, unsigned h)
{
    if (view_ok && tex_view.surface.width == w && tex_view.surface.height == h)
        return 1;
    memset(&tex_view, 0, sizeof tex_view);
    tex_view.surface.width = w;
    tex_view.surface.height = h;
    tex_view.surface.depth = 1;
    tex_view.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    tex_view.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    tex_view.surface.use = GX2_SURFACE_USE_TEXTURE;
    tex_view.surface.mipLevels = 1;
    tex_view.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    tex_view.viewNumSlices = 1;
    tex_view.compMap = 0x00010203;
    GX2CalcSurfaceSizeAndAlignment(&tex_view.surface);
    pad_surface(&tex_view.surface);
    if (tex_view.surface.imageSize > tex.surface.imageSize) {
        view_ok = 0;
        return 0;
    }
    tex_view.surface.image = tex.surface.image;
    GX2InitTextureRegs(&tex_view);
    view_ok = 1;
    return 1;
}

int gx2_gs_set_texture_rect(const uint32_t *argb, unsigned w, unsigned h, unsigned rx0, unsigned ry0, unsigned rx1,
                            unsigned ry1, int has_alpha, int linear, int wrap)
{
    if (!ready || !argb || !w || !h || w > tw_max || h > th_max || rx1 > w || ry1 > h || rx0 >= rx1 || ry0 >= ry1)
        return 0;
    if (tex_pending_use) {
        /* One texture slot: the GPU must be done with queued draws that read it. */
        gpu_wait();
        bind_state();
    }
    if (!prepare_view(w, h))
        return 0;
    uint32_t *dst = (uint32_t *)tex_view.surface.image;
    const unsigned pitch = tex_view.surface.pitch;
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
    mark_rows(dst, pitch, w, ry0, ry1);
    /* Flush from the base address (Cemu looks the texture up by it) to the last rewritten row. */
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, tex_view.surface.image, (uint32_t)(ry1 * pitch * 4u));
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

void gx2_gs_mark_texture_used(void) { tex_pending_use = 1; }

uint32_t *gx2_gs_target_cpu(unsigned w, unsigned h, unsigned *pitch)
{
    if (!ready || !w || !h || w > target.surface.width || h > target.surface.height)
        return NULL;
    gpu_wait();
    cur_w = w;
    cur_h = h;
    if (pitch)
        *pitch = utex.surface.pitch;
    return (uint32_t *)utex.surface.image;
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
    /* The CPU filled rows y0..y1 of the staging texture; the GPU copies them onto the target with a quad (no
     * blending, no depth). Writing the colour buffer memory directly is not seen by Cemu, whose render targets live
     * in host textures. */
    mark_rows((uint32_t *)utex.surface.image, utex.surface.pitch, utex.surface.width, y0, y1);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, utex.surface.image, (uint32_t)(y1 * utex.surface.pitch * 4u));
    if (vb_used + 4 > VB_MAX) {
        gpu_wait();
        bind_state();
        gx2_gs_set_blend(GX2_GS_BLEND_OFF, 0x80);
    }
    GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
    GX2SetPixelTexture(&utex, sh_mod.pixelShader->samplerVars[0].location);
    GX2SetPixelSampler(&samplers[0][0], sh_mod.pixelShader->samplerVars[0].location);
    gx2_gs_vertex q[4];
    const float ya = 1.0f - 2.0f * (float)y0 / (float)cur_h, yb = 1.0f - 2.0f * (float)y1 / (float)cur_h;
    const float ue = (float)cur_w / (float)utex.surface.width;
    const float va = (float)y0 / (float)utex.surface.height, vb2 = (float)y1 / (float)utex.surface.height;
    const float px[4] = { -1, 1, 1, -1 }, py[4] = { ya, ya, yb, yb }, u[4] = { 0, ue, ue, 0 }, v[4] = { va, va, vb2, vb2 };
    for (int i = 0; i < 4; i++) {
        q[i].pos[0] = px[i]; q[i].pos[1] = py[i]; q[i].pos[2] = 0.0f; q[i].pos[3] = 1.0f;
        q[i].col[0] = q[i].col[1] = q[i].col[2] = q[i].col[3] = 1.0f;
        q[i].uv[0] = u[i]; q[i].uv[1] = v[i]; q[i].uv[2] = 0.0f; q[i].uv[3] = 1.0f;
    }
    gx2_gs_vertex *dst = vb + vb_used;
    memcpy(dst, q, sizeof q);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, dst, sizeof q);
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
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, dst, count * sizeof *v);
    WHBGfxShaderGroup *g = textured ? &sh_mod : &sh_color;
    GX2SetFetchShader(&g->fetchShader);
    GX2SetVertexShader(g->vertexShader);
    GX2SetPixelShader(g->pixelShader);
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
    if (swap) {
        uint32_t *w = (uint32_t *)ztex.surface.image;
        for (unsigned y = y0; y < y1; y++)
            for (unsigned x = 0; x < cur_w; x++)
                w[y * ztex.surface.pitch + x] = swap32(w[y * ztex.surface.pitch + x]);
    }
    mark_rows((uint32_t *)ztex.surface.image, ztex.surface.pitch, ztex.surface.width, y0, y1);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, ztex.surface.image, (uint32_t)(y1 * ztex.surface.pitch * 4u));
    bind_state();
    GX2SetColorControl(GX2_LOGIC_OP_COPY, 0, FALSE, FALSE);
    GX2SetDepthOnlyControl(TRUE, TRUE, GX2_COMPARE_FUNC_ALWAYS);
    /* The full-size upload texture stays as initialised; the quad's u,v select the cur_w x cur_h corner. */
    GX2SetPixelTexture(&ztex, sh_zload.pixelShader->samplerVars[0].location);
    GX2SetPixelSampler(&samplers[0][0], sh_zload.pixelShader->samplerVars[0].location);
    gx2_gs_vertex q[4];
    const float ya = 1.0f - 2.0f * (float)y0 / (float)cur_h, yb = 1.0f - 2.0f * (float)y1 / (float)cur_h;
    const float ue = (float)cur_w / (float)ztex.surface.width;
    const float va = (float)y0 / (float)ztex.surface.height, vb2 = (float)y1 / (float)ztex.surface.height;
    const float px[4] = { -1, 1, 1, -1 }, py[4] = { ya, ya, yb, yb }, u[4] = { 0, ue, ue, 0 }, v[4] = { va, va, vb2, vb2 };
    for (int i = 0; i < 4; i++) {
        q[i].pos[0] = px[i]; q[i].pos[1] = py[i]; q[i].pos[2] = 0.0f; q[i].pos[3] = 1.0f;
        q[i].col[0] = q[i].col[1] = q[i].col[2] = q[i].col[3] = 1.0f;
        q[i].uv[0] = u[i]; q[i].uv[1] = v[i]; q[i].uv[2] = 0.0f; q[i].uv[3] = 1.0f;
    }
    gx2_gs_vertex *dst = vb + vb_used;
    memcpy(dst, q, sizeof q);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, dst, sizeof q);
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
    pad_surface(&ztex.surface);
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
