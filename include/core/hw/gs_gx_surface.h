#ifndef PCSX2WII_GS_GX_SURFACE_H
#define PCSX2WII_GS_GX_SURFACE_H
#include "gs_gx.h"
#define GS_GX_SURFACE_PIXELS (640u*512u)
typedef struct {
 uint32_t bp,bw,width,height,active,dirty_pixels;
 uint8_t dirty[GS_GX_SURFACE_PIXELS],alpha[GS_GX_SURFACE_PIXELS];
} gs_gx_surface;
int gs_gx_surface_begin(gs_gx_surface *s,uint32_t bp,uint32_t bw,uint32_t width,uint32_t height);
int gs_gx_surface_mark(gs_gx_surface *s,const gs_gx_flat_draw *d);
int gs_gx_surface_import(gs_gx_surface *s,uint8_t *vram,uint32_t size,const void *pixels,uint32_t capacity);
/* Exact CPU depth planning for resident RGB. Reject aliasing and fragmented
 * rows before changing coverage or Z. Colors are neither read nor shaded. */
int gs_gx_surface_depth_range(const gs_gx_texture_draw *d,const gs_gx_pipeline *p,
 uint32_t color_lo,uint32_t color_hi,uint32_t *lo,uint32_t *hi);
int gs_gx_surface_depth_clip(gs_gx_texture_draw *d,const gs_gx_pipeline *p,
 const uint8_t *depth,uint32_t lo,uint32_t bytes,uint64_t *tested,uint64_t *failed);
#endif
