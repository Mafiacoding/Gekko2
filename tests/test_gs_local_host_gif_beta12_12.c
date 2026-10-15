#include <stdio.h>
#include "hw/gs_mem.c"
#include "hw/gif.c"
int main(void)
{
    uint8_t qword[32]={0};
    gs_mem_init();gif_init();
    gs_mem_write_psmct32_blk(0,64,2,3,0x12345678u);
    gs_mem_write_psmct32_blk(0,64,3,3,0xabcdef98u);
    apply_ad_write(GS_REG_BITBLTBUF,(1u<<16),0u);
    apply_ad_write(GS_REG_TRXPOS,2u|(3u<<16),0u);
    apply_ad_write(GS_REG_TRXREG,2u,1u);
    apply_ad_write(GS_REG_TRXDIR,TRXDIR_LOCAL_TO_HOST,0);
    if(gif_read_local_fifo(qword,1u)!=1u)return puts("FAIL GIF read FIFO"),1;
    if(qword[0]!=0x78u||qword[1]!=0x56u||qword[2]!=0x34u||qword[3]!=0x12u||
       qword[4]!=0x98u||qword[5]!=0xefu||qword[6]!=0xcdu||qword[7]!=0xabu)
       return puts("FAIL GIF FIFO little endian bytes"),2;
    if(gif_local_host_stat(0)!=1 || gif_local_host_stat(1)!=16 ||
       g_gif.trx_xdir!=TRXDIR_OFF)return puts("FAIL GIF completion"),3;
    if(gif_read_local_fifo(qword,1u)!=0)return puts("FAIL post-completion read"),4;
    apply_ad_write(GS_REG_TRXDIR,TRXDIR_LOCAL_TO_HOST,0);
    if(gif_read_local_fifo(qword,1u)!=1)return puts("FAIL repeat transfer"),5;
    puts("PASS GIF A+D TRXDIR=1 local-to-host read FIFO");return 0;
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
