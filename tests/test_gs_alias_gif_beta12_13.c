#include <stdio.h>
#include "hw/gs_mem.c"
#include "hw/gif.c"
static unsigned alias_calls;
static int alias_resolver(void *opaque,uint8_t *vram,uint32_t size)
{(void)opaque;(void)vram;(void)size;alias_calls++;return 1;}
int main(void)
{
    gs_mem_init();gif_init();
    gs_mem_write_psmct32_blk(0,64,2,3,0xffaa33bbu);
    /* Source BP=0, destination BP=128, both FBW=1*64, PSM=0. */
    apply_ad_write(GS_REG_BITBLTBUF,(1u<<16),128u|(1u<<16));
    apply_ad_write(GS_REG_TRXPOS,2u|(3u<<16),5u|(7u<<16));
    apply_ad_write(GS_REG_TRXREG,1,1);
    apply_ad_write(GS_REG_TRXDIR,TRXDIR_LOCAL_TO_LOCAL,0);
    if(gs_mem_read_psmct32_blk(128,64,5,7)!=0xffaa33bbu ||
       gif_local_copy_stat(0)!=1 || gif_local_copy_stat(2)!=1 ||
       g_gif.trx_xdir!=TRXDIR_OFF || g_gif.trx_active)return puts("FAIL GIF trigger"),1;
    /* No nonsensical memory writes for unsupported conversions. */
    apply_ad_write(GS_REG_BITBLTBUF,(1u<<16),128u|(1u<<16)|(2u<<24));
    apply_ad_write(GS_REG_TRXDIR,TRXDIR_LOCAL_TO_LOCAL,0);
    if(gif_local_copy_stat(1)!=1)return puts("FAIL unsupported FORMAT counter"),2;
    /* Explicit native page extents: smaller indexed PSMs must not use
     * the nominal 32-bit framebuffer geometry. */
    if(gs_page_end(0,128,128,64,0x13u)!=8192u ||
       gs_page_end(0,128,128,128,0x14u)!=8192u ||
       gs_page_end(0,128,128,64,2u)!=16384u ||
       gs_page_end(0,128,128,64,0u)!=32768u)
       return puts("FAIL native PSM page bounds"),4;
    /* Physically aliased texture samples must resolve deferred GX data.
     * Disjoint indexed samples must not force a needless GPU readback. */
    if(!gs_mem_gpu_bind(alias_resolver,&alias_calls))return puts("FAIL bind"),5;
    if(!gs_mem_gpu_protect_range(0,8192)||!gs_mem_gpu_mark_pending())
       return puts("FAIL pending"),6;
    g_gx_sprite_textured=1;
    g_gif.tex_psm=0x13;g_gif.tex_tbw=128;
    g_gif.tex_tw=7;g_gif.tex_th=6;g_gif.tex_tbp0=32u;
    gs_gx_sprite_key();
    if(alias_calls!=0||!gs_mem_gpu_pending()||gs_mem_alias_stat(2)==0)
       return puts("FAIL disjoint indexed source"),7;
    g_gif.tex_tbp0=0u;
    gs_gx_sprite_key();
    if(alias_calls!=1||gs_mem_gpu_pending()||gs_mem_alias_stat(1)==0)
       return puts("FAIL aliased indexed source readback"),8;
    puts("PASS GIF TRXDIR=2, PSM page extents and GPU texture alias coherence");
    return 0;
}
/* Non-GX test: the real GIF handlers reference render entry points
 * even though this fixture only exercises register transfers. */
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
