#include "core/hw/gs_gx.h"
#include "core/hw/gs.h"
#include "core/hw/vif.h"
#include "core/hw/ee_intc.h"
#include "core/ee/ee_core.h"
#include "core/recompiler/vu_jit.h"
int gs_gx_render_active(void){return 0;}
int gs_gx_draw_flat_pipeline(uint32_t a,uint32_t b,uint32_t c,uint32_t d,
    int32_t e,int32_t f,int32_t g,int32_t h,const int32_t *i,
    uint32_t j,uint32_t k,const gs_gx_pipeline *l){return 0;}
int gs_gx_draw_flat_psm(uint32_t a,uint32_t b,uint32_t c,uint32_t d,
    int32_t e,int32_t f,int32_t g,int32_t h,const int32_t *i,uint32_t j,uint32_t k){return 0;}
int gs_gx_draw_shaded_triangle(uint32_t a,uint32_t b,uint32_t c,
    int32_t d,int32_t e,int32_t f,int32_t g,const int32_t *h,
    uint32_t i,gs_gx_texel_fn j,const gs_gx_pipeline *k){return 0;}
int gs_gx_draw_gouraud_triangle(uint32_t a,uint32_t b,uint32_t c,
    int32_t d,int32_t e,int32_t f,int32_t g,const int32_t *h,
    const uint32_t *i,uint32_t j,const gs_gx_pipeline *k){return 0;}
int gs_gx_draw_texture_sprite(uint32_t a,uint32_t b,uint32_t c,
    int32_t d,int32_t e,uint32_t f,uint32_t g,const int32_t *h,const int32_t *i,
    double j,double k,uint32_t l,gs_gx_texel_fn m,const gs_gx_pipeline *n){return 0;}
int gs_gx_draw_uv_decal_triangle(uint32_t a,uint32_t b,uint32_t c,
    int32_t d,int32_t e,int32_t f,int32_t g,const int32_t *h,const float *i,
    uint32_t j,uint32_t k,uint32_t l,uint32_t m,gs_gx_texel_fn n){return 0;}
void gs_gx_source_key(uint32_t a,uint32_t b,int c){}
int gs_raise_event(unsigned a){return 0;}
void ee_intc_raise(int a){}

static ee_state_t ee_dummy;
ee_state_t *ee_core_get_state(void){return &ee_dummy;}
static vif_state_t v0,v1;
vif_state_t *vif0_get_state(void){return &v0;}
vif_state_t *vif1_get_state(void){return &v1;}
int vu_jit_try_upper(uint32_t vf[32][4],uint32_t *vi,uint32_t acc[4],uint32_t w){(void)vf;(void)vi;(void)acc;(void)w;return 0;}
int vu_jit_try_lower(uint32_t vf[32][4],uint32_t *vi,uint8_t *mem,uint32_t mask,uint32_t w,uint32_t pc,uint32_t *delay,uint32_t *target){(void)vf;(void)vi;(void)mem;(void)mask;(void)w;(void)pc;(void)delay;(void)target;return 0;}
int vu_jit_try_pair(uint32_t vf[32][4],uint32_t *vi,uint32_t *acc,uint8_t *mem,uint32_t mask,uint32_t pc,uint32_t *delay,uint32_t *target,uint32_t upper,uint32_t lower){(void)vf;(void)vi;(void)acc;(void)mem;(void)mask;(void)pc;(void)delay;(void)target;(void)upper;(void)lower;return 0;}
unsigned vu_jit_try_timed_block(vu_jit_timed_context *c,unsigned budget){(void)c;(void)budget;return 0;}
