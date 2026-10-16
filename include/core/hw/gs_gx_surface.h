#ifndef PCSX2WII_GS_GX_SURFACE_H
#define PCSX2WII_GS_GX_SURFACE_H
#include "gs_gx.h"
#define GS_GX_SURFACE_PIXELS (640u*512u)
typedef struct {
 uint32_t bp,bw,width,height,active,dirty_pixels;
 uint8_t dirty[GS_GX_SURFACE_PIXELS],alpha[GS_GX_SURFACE_PIXELS];
 uint32_t dirty_x0,dirty_y0,dirty_x1,dirty_y1;
} gs_gx_surface;
int gs_gx_surface_begin(gs_gx_surface *s,uint32_t bp,uint32_t bw,uint32_t width,uint32_t height);
int gs_gx_surface_mark(gs_gx_surface *s,const gs_gx_flat_draw *d);
int gs_gx_surface_import(gs_gx_surface *s,uint8_t *vram,uint32_t size,const void *pixels,uint32_t capacity);
int gs_gx_surface_import_rect(gs_gx_surface *s,uint8_t *vram,uint32_t size,const void *pixels,uint32_t capacity,
 uint32_t x,uint32_t y,uint32_t w,uint32_t h);
/* Exact CPU depth planning for resident RGB. Reject aliasing and fragmented
 * rows before changing coverage or Z. Colors are neither read nor shaded. */
int gs_gx_surface_depth_range(const gs_gx_texture_draw *d,const gs_gx_pipeline *p,
 uint32_t color_lo,uint32_t color_hi,uint32_t *lo,uint32_t *hi);
int gs_gx_surface_depth_clip(gs_gx_texture_draw *d,const gs_gx_pipeline *p,
 const uint8_t *depth,uint32_t lo,uint32_t bytes,uint64_t *tested,uint64_t *failed);
enum { GS_GX_WRITE_COLOR=1,GS_GX_WRITE_ALPHA=2,GS_GX_WRITE_Z=4,GS_GX_DO_BLEND=8 };
/* Pure GS fragment permissions. Incoming Z is raster-clamped, not truncated. */
uint32_t gs_gx_fragment_actions(const gs_gx_pipeline *p,uint32_t color_psm,
 uint32_t destination_alpha,uint32_t source_alpha,uint32_t z,uint32_t stored_z);
int gs_gx_native_texture_valid(uint32_t width,uint32_t height);
#endif
