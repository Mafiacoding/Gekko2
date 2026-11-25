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
static uint64_t stats[5];

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
    GX2InitTextureRegs(&tex);
    tex.surface.image = MEMAllocFromDefaultHeapEx(tex.surface.imageSize, tex.surface.alignment);
    if (!tex.surface.image)
        goto fail;
    memset(tex.surface.image, 0, tex.surface.imageSize);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, tex.surface.image, tex.surface.imageSize);

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
    if (target.surface.image) MEMFreeToDefaultHeap(target.surface.image);
    if (ctx) MEMFreeToDefaultHeap(ctx);
    vb = NULL;
    ctx = NULL;
    memset(&tex, 0, sizeof tex);
    memset(&target, 0, sizeof target);
    if (sh_color.vertexShader || sh_color.pixelShader) WHBGfxFreeShaderGroup(&sh_color);
    if (sh_mod.vertexShader || sh_mod.pixelShader) WHBGfxFreeShaderGroup(&sh_mod);
    memset(&sh_color, 0, sizeof sh_color);
    memset(&sh_mod, 0, sizeof sh_mod);
    ready = 0;
}

static void bind_state(void)
{
    GX2SetContextState(ctx);
    GX2SetColorBuffer(&target, GX2_RENDER_TARGET_0);
    GX2SetViewport(0.0f, 0.0f, (float)target.surface.width, (float)target.surface.height, 0.0f, 1.0f);
    GX2SetScissor(0, 0, target.surface.width, target.surface.height);
    GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
    GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, FALSE, FALSE);
    cur_blend = -1;
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
    if (!ready || !argb || !w || !h || w > tw_max || h > th_max)
        return 0;
    if (tex_pending_use) {
        /* One texture slot: the GPU must be done with queued draws that read it. */
        gpu_wait();
        bind_state();
    }
    uint32_t *dst = (uint32_t *)tex.surface.image;
    const unsigned pitch = tex.surface.pitch;
    for (unsigned y = 0; y < h; y++) {
        uint32_t *row = dst + (size_t)y * pitch;
        const uint32_t *src = argb + (size_t)y * w;
        for (unsigned x = 0; x < w; x++) {
            const uint32_t c = src[x];
            /* GS texture alpha 0x80 = 1.0; the shader multiplies, so scale it to the 0..255 = 0..2 range halfway. */
            uint32_t a = has_alpha ? (c >> 24) * 2u : 0xffu;
            if (a > 0xffu) a = 0xffu;
            row[x] = (c << 8) | a;
        }
    }
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, tex.surface.image, tex.surface.imageSize);
    /* Sample only the used part: the caller's u,v are relative to w x h, scale them via the texture size. */
    tex.surface.width = w;
    tex.surface.height = h;
    GX2InitTextureRegs(&tex);
    GX2SetPixelTexture(&tex, sh_mod.pixelShader->samplerVars[0].location);
    GX2SetPixelSampler(&samplers[linear ? 1 : 0][wrap ? 1 : 0], sh_mod.pixelShader->samplerVars[0].location);
    tex.surface.width = tw_max;
    tex.surface.height = th_max;
    stats[2]++;
    return 1;
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
    if (!ready)
        return NULL;
    GX2Flush();
    gpu_wait();
    /* GPU writes went to memory; drop stale CPU cache lines before reading. */
    DCInvalidateRange(target.surface.image, target.surface.imageSize);
    if (pitch)
        *pitch = target.surface.pitch;
    return (const uint32_t *)target.surface.image;
}
