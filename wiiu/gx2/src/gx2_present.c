/* GPL-3.0-or-later. See gx2_present.h. */
#include "gx2_present.h"
#include <coreinit/cache.h>
#include <coreinit/memdefaultheap.h>
#include <gx2/draw.h>
#include <gx2/enum.h>
#include <gx2/mem.h>
#include <gx2/sampler.h>
#include <gx2/shaders.h>
#include <gx2/surface.h>
#include <gx2/texture.h>
#include <whb/gfx.h>
#include <string.h>

static WHBGfxShaderGroup group;
static GX2Texture texture;
static GX2Sampler sampler_point, sampler_linear;
static float *pos4, *tex4; /* 4 vertices each: x,y */
static unsigned tex_w, tex_h;
static int ready, gfx_up;

/* 4:3 picture inside a 16:9 output: x extent +-0.75 of the width, full height. Texture v runs top to bottom. */
static const float quad_pos[8] = { -0.75f, 1.0f, 0.75f, 1.0f, 0.75f, -1.0f, -0.75f, -1.0f };

int gx2_present_init(const void *gsh, uint32_t size, unsigned max_w, unsigned max_h)
{
    (void)size;
    if (ready || !gsh || !max_w || !max_h)
        return 0;
    if (!WHBGfxInit())
        return 0;
    gfx_up = 1;
    memset(&group, 0, sizeof group);
    if (!WHBGfxLoadGFDShaderGroup(&group, 0, gsh))
        goto fail;
    if (!WHBGfxInitShaderAttribute(&group, "aPosition", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32) ||
        !WHBGfxInitShaderAttribute(&group, "aTexCoord", 1, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32) ||
        !WHBGfxInitFetchShader(&group))
        goto fail;

    memset(&texture, 0, sizeof texture);
    texture.surface.width = max_w;
    texture.surface.height = max_h;
    texture.surface.depth = 1;
    texture.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    texture.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    texture.surface.use = GX2_SURFACE_USE_TEXTURE;
    texture.surface.mipLevels = 1;
    texture.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    texture.viewNumSlices = 1;
    texture.compMap = 0x00010203; /* R,G,B,A straight */
    GX2CalcSurfaceSizeAndAlignment(&texture.surface);
    GX2InitTextureRegs(&texture);
    texture.surface.image = MEMAllocFromDefaultHeapEx(texture.surface.imageSize, texture.surface.alignment);
    if (!texture.surface.image)
        goto fail;
    memset(texture.surface.image, 0, texture.surface.imageSize);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, texture.surface.image, texture.surface.imageSize);

    GX2InitSampler(&sampler_point, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_POINT);
    GX2InitSampler(&sampler_linear, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);

    pos4 = MEMAllocFromDefaultHeapEx(8 * sizeof(float), GX2_VERTEX_BUFFER_ALIGNMENT);
    tex4 = MEMAllocFromDefaultHeapEx(8 * sizeof(float), GX2_VERTEX_BUFFER_ALIGNMENT);
    if (!pos4 || !tex4)
        goto fail;
    memcpy(pos4, quad_pos, sizeof quad_pos);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, pos4, 8 * sizeof(float));
    tex_w = max_w;
    tex_h = max_h;
    ready = 1;
    return 1;
fail:
    gx2_present_shutdown();
    return 0;
}

void gx2_present_shutdown(void)
{
    if (texture.surface.image)
        MEMFreeToDefaultHeap(texture.surface.image);
    if (pos4)
        MEMFreeToDefaultHeap(pos4);
    if (tex4)
        MEMFreeToDefaultHeap(tex4);
    memset(&texture, 0, sizeof texture);
    pos4 = tex4 = NULL;
    if (group.vertexShader || group.pixelShader)
        WHBGfxFreeShaderGroup(&group);
    memset(&group, 0, sizeof group);
    if (gfx_up)
        WHBGfxShutdown();
    gfx_up = ready = 0;
}

static void draw_to(int tv, const GX2Sampler *sampler)
{
    if (tv)
        WHBGfxBeginRenderTV();
    else
        WHBGfxBeginRenderDRC();
    WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    GX2SetFetchShader(&group.fetchShader);
    GX2SetVertexShader(group.vertexShader);
    GX2SetPixelShader(group.pixelShader);
    GX2SetPixelTexture(&texture, group.pixelShader->samplerVars[0].location);
    GX2SetPixelSampler(sampler, group.pixelShader->samplerVars[0].location);
    GX2SetAttribBuffer(0, 8 * sizeof(float), 2 * sizeof(float), pos4);
    GX2SetAttribBuffer(1, 8 * sizeof(float), 2 * sizeof(float), tex4);
    GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
    if (tv)
        WHBGfxFinishRenderTV();
    else
        WHBGfxFinishRenderDRC();
}

void gx2_present_frame(const uint32_t *xrgb, unsigned w, unsigned h, int filter)
{
    if (!ready || !xrgb || !w || !h || w > tex_w || h > tex_h)
        return;
    /* UNORM_R8_G8_B8_A8 in big-endian memory is the word 0xRRGGBBAA: shift the colour up, alpha opaque. */
    uint32_t *dst = (uint32_t *)texture.surface.image;
    const unsigned pitch = texture.surface.pitch;
    for (unsigned y = 0; y < h; y++) {
        uint32_t *row = dst + (size_t)y * pitch;
        const uint32_t *src = xrgb + (size_t)y * w;
        for (unsigned x = 0; x < w; x++)
            row[x] = (src[x] << 8) | 0xffu;
    }
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, texture.surface.image, texture.surface.imageSize);
    /* Only the used part of the texture is visible: u,v run 0..w/tex_w and 0..h/tex_h. */
    const float u = (float)w / (float)tex_w, v = (float)h / (float)tex_h;
    const float uv[8] = { 0.0f, 0.0f, u, 0.0f, u, v, 0.0f, v };
    memcpy(tex4, uv, sizeof uv);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, tex4, 8 * sizeof(float));
    const GX2Sampler *s = filter ? &sampler_linear : &sampler_point;
    WHBGfxBeginRender();
    draw_to(1, s);
    draw_to(0, s);
    WHBGfxFinishRender();
}
