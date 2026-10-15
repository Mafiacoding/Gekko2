#ifndef GEKKO2_GX_ADDITIVE_H
#define GEKKO2_GX_ADDITIVE_H
#include "gs_gx.h"
int gs_gx_additive_supported(const gs_gx_pipeline *p);
uint32_t gs_gx_additive_pixel(uint32_t rgba,uint32_t fix);
uint64_t gs_gx_additive_stat(unsigned n);
#endif
