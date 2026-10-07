#ifndef PCSX2WII_GS_GX_H
#define PCSX2WII_GS_GX_H
#include <stdint.h>
/* GX RGBA8 tiled presentation copy of current PSMCT32/24 RGB VRAM. Returns bytes,
 * or zero on unsupported/bounded input. Does not render PS2 primitives. */
uint32_t gs_gx_pack_rgba8(void *dst,uint32_t capacity,uint32_t bp,uint32_t bw,
                        uint32_t sx,uint32_t sy,uint32_t width,uint32_t height);
/* Import exact tiled GX RGBA8 RGB with a known GS alpha into shared VRAM.
 * Full destination validation precedes writes. No display broadcast clamp. */
int gs_gx_unpack_psmct32(uint8_t *vram,uint32_t size,const void *rgba8,uint32_t capacity,
                       uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,
                       uint32_t width,uint32_t height,uint32_t alpha);
/* Constant-time PSMCT32 target bounds, validated before any writes. */
int gs_gx_target_valid(uint32_t size,uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,
                       uint32_t width,uint32_t height);
typedef struct {
    uint32_t bp,bw,x,y,width,height,rgba;
    uint32_t psm; /* 0 CT32 writes RGBA, 1 CT24 preserves destination alpha. */
    uint16_t left[512],right[512]; /* relative, right exclusive; empty = 0,0 */
} gs_gx_flat_draw;
/* kind 3 triangle (inclusive GS integer coverage), kind 6 sprite (exclusive).
 * Caller supplies clipped bbox; only opaque flat PSMCT32 without Z accepted. */
int gs_gx_prepare_flat(gs_gx_flat_draw *draw,uint32_t kind,uint32_t bp,uint32_t bw,
                      int32_t minx,int32_t miny,int32_t maxx,int32_t maxy,
                      const int32_t *xy,uint32_t rgba,uint32_t scanmsk);
int gs_gx_import_flat(uint8_t *vram,uint32_t size,const void *rgba8,uint32_t capacity,
                      const gs_gx_flat_draw *draw);
/* R1298 texture snapshot: decoded/GS-shaded texels, GX nearest lookup.
 * The CPU maps exact GS sample indices; axis fitting requires >=1/8 texel
 * margin at every output pixel. Alpha remains exact CPU side metadata. */
typedef uint32_t (*gs_gx_texel_fn)(int32_t x,int32_t y);
typedef struct {
    uint32_t zbp,zpsm,ztest,zwrite,ztst,z;
    uint32_t blend,a,b,c,d,fix,pabe,colclamp;
} gs_gx_pipeline;
typedef struct {
    gs_gx_flat_draw coverage;
    int32_t origin_x,origin_y;
    uint32_t tw,th,columns,rows;
    int32_t column[640],row[512];
    float s0,s1,t0,t1;
    gs_gx_pipeline pipeline;
    uint32_t hardware_depth,hardware_blend; /* depth: 0 hybrid, 1 direct, 2 split Z32 */
} gs_gx_texture_draw;
int gs_gx_prepare_texture(gs_gx_texture_draw *d,uint32_t psm,uint32_t bp,uint32_t bw,
    int32_t x,int32_t y,uint32_t w,uint32_t h,const int32_t *columns,const int32_t *rows,
    double step_x,double step_y,uint32_t scanmsk);
uint32_t gs_gx_pack_texture(void *out,uint32_t capacity,const gs_gx_texture_draw *d,gs_gx_texel_fn sample);
int gs_gx_import_texture(uint8_t *vram,uint32_t size,const void *pixels,uint32_t capacity,
    const void *source,uint32_t source_capacity,const gs_gx_texture_draw *d);
int gs_gx_draw_texture_sprite(uint32_t psm,uint32_t bp,uint32_t bw,
    int32_t x,int32_t y,uint32_t w,uint32_t h,const int32_t *columns,const int32_t *rows,
    double step_x,double step_y,uint32_t scanmsk,gs_gx_texel_fn sample,const gs_gx_pipeline *pipeline);
int gs_gx_draw_flat_pipeline(uint32_t psm,uint32_t kind,uint32_t bp,uint32_t bw,
    int32_t minx,int32_t miny,int32_t maxx,int32_t maxy,const int32_t *xy,
    uint32_t rgba,uint32_t scanmsk,const gs_gx_pipeline *pipeline);
/* R1330-E: native GX vertex-color interpolation for untextured Gouraud triangles.
 * Unsupported GS tests/effects return 0 so the scalar rasterizer remains exact. */
int gs_gx_draw_gouraud_triangle(uint32_t psm,uint32_t bp,uint32_t bw,
    int32_t minx,int32_t miny,int32_t maxx,int32_t maxy,const int32_t *xy,
    const uint32_t *rgba,uint32_t scanmsk,const gs_gx_pipeline *pipeline);
int gs_gx_draw_uv_decal_triangle(uint32_t psm,uint32_t bp,uint32_t bw,
    int32_t minx,int32_t miny,int32_t maxx,int32_t maxy,const int32_t *xy,const float *uv,
    uint32_t tex_w,uint32_t tex_h,uint32_t alpha,uint32_t scanmsk,gs_gx_texel_fn sample);
int gs_gx_draw_mapped_triangle(uint32_t psm,uint32_t bp,uint32_t bw,int32_t x,int32_t y,uint32_t w,uint32_t h,
 const int32_t *xy,const int32_t *columns,const int32_t *rows,double du,double dv,uint32_t scanmsk,
 gs_gx_texel_fn sample,const gs_gx_pipeline *pipeline);
void gs_gx_set_render_enabled(int enabled);
int gs_gx_ready(void);
int gs_gx_render_active(void);
/* 0=accepted draws, 1=quads, 2=readback bytes, 3=resolve waits, 4=attempts. */
uint64_t gs_gx_work_count(unsigned index);
/* 0 candidates, 1 accepted textured sprites, 2 uploaded bytes, 3 layout rejects. */
uint64_t gs_gx_texture_count(unsigned index);
/* 0 depth hardware, 1 depth hybrid, 2 blend hardware, 3 blend hybrid,
 * 4 split Z32 draws, 5 split PABE draws. Hardware depth has CPU shadow resolve. */
uint64_t gs_gx_pipeline_count(unsigned index);
/* 0 opens, 1 batched draws, 2 resolves, 3 GPU snapshots, 4 resident presents, 5 seed bytes. */
uint64_t gs_gx_surface_count(unsigned index);
/* Diagnostic escape hatch; synchronization must succeed before changing mode. */
int gs_gx_set_residency_enabled(int enabled);
/* Caller verifies current physical source/CLUT/state; key consumed by one draw. */
void gs_gx_source_key(uint32_t lo,uint32_t hi,int valid);
uint64_t gs_gx_source_cache_count(unsigned index);
/* CT24 uses the same swizzle and writes RGB only. */
int gs_gx_draw_flat_psm(uint32_t psm,uint32_t kind,uint32_t bp,uint32_t bw,
                    int32_t minx,int32_t miny,int32_t maxx,int32_t maxy,
                    const int32_t *xy,uint32_t rgba,uint32_t scanmsk);
int gs_gx_draw_flat(uint32_t kind,uint32_t bp,uint32_t bw,
                    int32_t minx,int32_t miny,int32_t maxx,int32_t maxy,
                    const int32_t *xy,uint32_t rgba,uint32_t scanmsk);
#ifdef GEKKO
#include <gccore.h>
int gs_gx_present(void *xfb,GXRModeObj *mode,uint32_t bp,uint32_t bw,
                  uint32_t sx,uint32_t sy,uint32_t width,uint32_t height);
/* Capture an already-rendered EFB rectangle (origin 0,0) for deferred VRAM
 * readback. Only aligned opaque RGB PSMCT32 targets; alpha is known GS data.
 * This queues a copy, not a draw. Caller must synchronize before changing EFB. */
int gs_gx_capture_vram_psmct32(uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,
                             uint32_t width,uint32_t height,uint32_t alpha);
void gs_gx_shutdown(void);
#endif
#endif
