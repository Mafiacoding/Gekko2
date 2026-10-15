#include "core/runtime_profile.h"
#include "core/recompiler/optimization.h"
/* Experimental GX presentation and gated flat GS triangle/sprite spans. */
#include "build_version.h"
#include "core/hw/gs_gx.h"
#include "core/hw/gs_mem.h"
#include "core/hw/gs_gx_dump.h"
#include "core/hw/gs_gx_additive.h"
#include "core/hw/gs_gx_surface.h"
#include "core/hw/gs_triangle_coverage.h"
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#ifdef GEKKO2_GX_DUMP_LAZY
#define GXD_FLUSH() ((void)0)
#else
#define GXD_FLUSH() gs_gx_dump_flush(NULL)
#endif
/* R1389-WD10: vertex accounting. Every GX_Begin declares a vertex count; a
 * mismatch with the positions actually written desyncs the GP command stream. */
static unsigned gx_vexp,gx_vcnt,gx_vline,gx_in_begin,gx_begins_since_wait,gx_last_line,gx_ctx_begins;static uint64_t gx_vmis[3];
static void gx_vtx_report(unsigned endline);
#define GX_Position3f32(x,y,z) (gx_vcnt++,GX_Position3f32((x),(y),(z)))
#define GX_End() do{gx_vtx_report(__LINE__);GX_End();}while(0)
#ifdef GEKKO2_GX_DUMP
#define GXB(t,f,n) do{unsigned gxb_n=(unsigned)(n);gs_gx_dump_record(gxb_n>65535u?"BEGIN_OVER16":"BEGIN",(uint32_t)(t),gxb_n,__LINE__,0);gx_vexp=(uint16_t)gxb_n;gx_vcnt=0;gx_vline=__LINE__;gx_last_line=__LINE__;gx_begins_since_wait++;gx_in_begin=1;GX_Begin((t),(f),(n));}while(0)
#else
#define GXB(t,f,n) do{unsigned gxb_n=(unsigned)(n);gx_vexp=(uint16_t)gxb_n;gx_vcnt=0;gx_vline=__LINE__;gx_last_line=__LINE__;gx_begins_since_wait++;gx_in_begin=1;GX_Begin((t),(f),(n));}while(0)
#endif

/* GX receives full-range RGB. Its display-copy engine converts to XFB
 * YCbCr; applying a studio-range clamp here raises RGB black to gray. */
static uint8_t scanout_rgb(uint32_t x) {return (uint8_t)x;}
static uint64_t additive_counts[3];
uint64_t gs_gx_additive_stat(unsigned n){return n<3u?additive_counts[n]:0;}
__attribute__((noinline,noclone)) uint32_t gs_gx_pack_rgba8_profile_impl(void *dst,uint32_t capacity,uint32_t bp,uint32_t bw,
                        uint32_t sx,uint32_t sy,uint32_t width,uint32_t height){
    if(!dst||!width||!height||width>1024u||height>512u)return 0;
    uint32_t tw=(width+3u)&~3u,th=(height+3u)&~3u,bytes=tw*th*4u;
    if(capacity<bytes)return 0;
    uint8_t *out=dst;uint32_t row[1024];
    for(uint32_t y=0;y<th;y++) {
        gs_mem_read_psmct32_span(row,bp,bw,sx,sy+(y<height?y:height-1u),width);
        uint32_t tile_row=(y/4u)*(tw/4u)*64u,local_y=(y&3u)*8u;
        /* Four adjacent pixels share one tile; advance between AR/GB planes
         * rather than recomputing division/modulo for every destination. */
        for(uint32_t x=0;x<tw;x+=4u) {
            uint8_t *ar=out+tile_row+(x/4u)*64u+local_y;
            for(uint32_t k=0;k<4u;k++) {
                uint32_t source=x+k;
                uint32_t p=row[source<width?source:width-1u];
                ar[k*2u]=255u;ar[k*2u+1u]=scanout_rgb(p);
                ar[32u+k*2u]=scanout_rgb(p>>8);ar[33u+k*2u]=scanout_rgb(p>>16);
            }
        }
    }
    return bytes;
}
__attribute__((noinline,noclone)) uint32_t gs_gx_pack_rgba8(void *dst,uint32_t capacity,uint32_t bp,uint32_t bw,
                        uint32_t sx,uint32_t sy,uint32_t width,uint32_t height)
{
 unsigned profile_previous=gp_enter(GP_GX_UPLOAD);
 uint32_t result=gs_gx_pack_rgba8_profile_impl(dst,capacity,bp,bw,sx,sy,width,height);
 gp_leave(profile_previous);
 return result;
}

static uint64_t target_offset(uint32_t bp,uint32_t bw,uint32_t x,uint32_t y)
{
    uint32_t pages=bw/64u;if(!pages)pages=1;
    uint32_t bx=(x&63u)>>3,by=(y&31u)>>3;
    uint32_t block=(bx&1u)|((by&1u)<<1)|((bx&2u)<<1)|((by&2u)<<2)|((bx&4u)<<2);
    uint32_t word=(x&1u)|((y&1u)<<1)|((x&6u)<<1)|((y&6u)<<3);
    return (uint64_t)bp*4u+((uint64_t)(y/32u)*pages+x/64u)*8192u+block*256u+word*4u;
}
int gs_gx_target_valid(uint32_t size,uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,
                        uint32_t width,uint32_t height)
{
    if(size!=GS_MEM_SIZE||!width||!height||width>640u||height>512u||
       (width&3u)||(height&3u)||x>2048u-width||y>2048u-height||
       !bw||bw>2048u||(bw&63u))return 0;
    /* PSMCT32 x/y bit contributions are monotonically increasing within
     * a page; crossing a page adds more than the reset contribution.
     * With bw/64 >= 1, the bottom-right address bounds every pixel.
     * Keep 64-bit arithmetic: a hostile bp must never wrap into VRAM. */
    return target_offset(bp,bw,x+width-1u,y+height-1u)<=size-4u;
}
int gs_gx_unpack_psmct32(uint8_t *vram,uint32_t size,const void *rgba8,uint32_t capacity,
                       uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,
                       uint32_t width,uint32_t height,uint32_t alpha)
{
    if(!vram||!rgba8||alpha>255u||!gs_gx_target_valid(size,bp,bw,x,y,width,height)||
       capacity<width*height*4u)return 0;
    const uint8_t *src=rgba8;
    for(uint32_t j=0;j<height;j++)for(uint32_t i=0;i<width;i++) {
        uint32_t tex=((j/4u)*(width/4u)+i/4u)*64u+(j&3u)*8u+(i&3u)*2u;
        uint32_t dst=(uint32_t)target_offset(bp,bw,x+i,y+j);
        vram[dst]=src[tex+1u];vram[dst+1u]=src[tex+32u];
        vram[dst+2u]=src[tex+33u];vram[dst+3u]=(uint8_t)alpha;
    }
    return 1;
}
static int render_enabled;
static int gx_wait_failed;
void gs_gx_set_render_enabled(int enabled){render_enabled=!!enabled&&!gx_wait_failed;}
static int64_t floor_div(int64_t n,int64_t d)
{int64_t q=n/d;return q-((n%d)<0);}
int gs_gx_prepare_flat(gs_gx_flat_draw *d,uint32_t kind,uint32_t bp,uint32_t bw,
                      int32_t minx,int32_t miny,int32_t maxx,int32_t maxy,
                      const int32_t *xy,uint32_t rgba,uint32_t scanmsk)
{
    if(!d||!xy||(kind!=3u&&kind!=6u)||minx<0||miny<0||maxx<minx||maxy<miny||
       maxx>2047||maxy>2047)return 0;
    uint32_t w=(uint32_t)(maxx-minx)+(kind==3u),h=(uint32_t)(maxy-miny)+(kind==3u);
    if(!w||!h||w>640u||h>512u)return 0;
    uint32_t tw=(w+3u)&~3u,th=(h+3u)&~3u;
    if(w>bw||(uint32_t)minx>bw-w||!gs_gx_target_valid(GS_MEM_SIZE,bp,bw,minx,miny,tw,th))return 0;
    int sign=1;
    if(kind==3u) {
        /* GS vertices may lie outside the viewport after XYOFFSET. The
         * clipped bbox stays bounded; signed edges are computed in int64. */
        for(unsigned n=0;n<6;n++)if(xy[n]<-8192||xy[n]>8191)return 0;
        int64_t area=(int64_t)(xy[2]-xy[0])*(xy[5]-xy[1])-(int64_t)(xy[3]-xy[1])*(xy[4]-xy[0]);
        if(!area)return 0;sign=area>0?1:-1;
    }
    memset(d,0,sizeof(*d));d->bp=bp;d->bw=bw;d->x=minx;d->y=miny;
    d->width=tw;d->height=th;d->rgba=rgba;
    for(uint32_t row=0;row<h;row++) {
        int64_t lo=minx,hi=(kind==3u?maxx:maxx-1);int32_t y=miny+(int32_t)row;
        if((scanmsk==2u&&!(y&1))||(scanmsk==3u&&(y&1)))continue;
        if(kind==3u)for(unsigned e=0;e<3;e++) {
            unsigned a=e*2,b=((e+1)%3)*2;
            int64_t dx=xy[b]-xy[a],dy=xy[b+1]-xy[a+1];
            int64_t A=-dy*sign,B=(dx*(y-xy[a+1])+dy*xy[a])*sign;
            if(!gs_triangle_edge_inclusive(xy[a],xy[a+1],xy[b],xy[b+1],sign))B--;
            if(A>0){int64_t bound=-floor_div(B,A);if(bound>lo)lo=bound;}
            else if(A<0){int64_t bound=floor_div(B,-A);if(bound<hi)hi=bound;}
            else if(B<0){hi=lo-1;break;}
        }
        if(lo<=hi){d->left[row]=(uint16_t)(lo-minx);d->right[row]=(uint16_t)(hi-minx+1);}
    }
    return 1;
}
int gs_gx_import_flat(uint8_t *vram,uint32_t size,const void *rgba8,uint32_t capacity,
                      const gs_gx_flat_draw *d)
{
    if(!vram||!rgba8||!d||d->psm>1u||!gs_gx_target_valid(size,d->bp,d->bw,d->x,d->y,d->width,d->height)||
       capacity<d->width*d->height*4u)return 0;
    for(uint32_t y=0;y<d->height;y++)if(d->left[y]>d->right[y]||d->right[y]>d->width)return 0;
    const uint8_t *src=rgba8;
    for(uint32_t y=0;y<d->height;y++)for(uint32_t x=d->left[y];x<d->right[y];x++) {
        uint32_t t=((y/4u)*(d->width/4u)+x/4u)*64u+(y&3u)*8u+(x&3u)*2u;
        uint32_t off=(uint32_t)target_offset(d->bp,d->bw,d->x+x,d->y+y);
        vram[off]=src[t+1u];vram[off+1u]=src[t+32u];vram[off+2u]=src[t+33u];if(d->psm==0u)vram[off+3u]=(uint8_t)(d->rgba>>24);
    }
    return 1;
}
/* Fit the exact CPU-selected indices with a GX nearest ramp. Pick the
 * middle of the intersection of all texel intervals, never a boundary.
 * Reject wrapping/clamping discontinuities or numerically narrow windows. */
static int texture_axis(const int32_t *map,uint32_t n,double step,int32_t origin,
                        uint32_t extent,float *a,float *b)
{
    if(!(step>=-2048.0 && step<=2048.0))return 0;
    double lo=-4096.0,hi=4096.0;
    for(uint32_t k=0;k<n;k++) {
        double base=(double)map[0]+step*k;
        double l=(double)map[k]-base,h=l+1.0;
        if(l>lo)lo=l;if(h<hi)hi=h;
    }
    if(!(hi-lo>=0.25))return 0;
    double center=(double)(map[0]-origin)+(lo+hi)*0.5;
    *a=(float)((center-step*0.5)/extent);
    *b=(float)((center+step*((double)n-0.5))/extent);
    return 1;
}
int gs_gx_prepare_texture(gs_gx_texture_draw *d,uint32_t psm,uint32_t bp,uint32_t bw,
    int32_t x,int32_t y,uint32_t w,uint32_t h,const int32_t *columns,const int32_t *rows,
    double step_x,double step_y,uint32_t scanmsk)
{
    if(!d||!columns||!rows||psm>1u||!w||!h||w>640u||h>512u||x<0||y<0||
       x>2048-(int32_t)w||y>2048-(int32_t)h)return 0;
    int32_t lx=columns[0],rx=lx,ly=rows[0],ry=ly;
    for(uint32_t k=0;k<w;k++){if(columns[k]<lx)lx=columns[k];if(columns[k]>rx)rx=columns[k];}
    for(uint32_t k=0;k<h;k++){if(rows[k]<ly)ly=rows[k];if(rows[k]>ry)ry=rows[k];}
    if(lx<-2048||rx>2047||ly<-2048||ry>2047||rx-lx>=1024||ry-ly>=512)return 0;
    uint32_t tw=((uint32_t)(rx-lx+1)+3u)&~3u,th=((uint32_t)(ry-ly+1)+3u)&~3u;
    /* Do not upload a huge source for a tiny destination. This is a drawing
     * path, not a claim that snapshot conversion is free or always faster. */
    if(tw*th>w*h)return 0;
    memset(d,0,sizeof(*d));
    int32_t xy[4]={x,y,x+(int32_t)w,y+(int32_t)h};
    if(!gs_gx_prepare_flat(&d->coverage,6,bp,bw,x,y,x+w,y+h,xy,0,scanmsk))return 0;
    if(!texture_axis(columns,w,step_x,lx,tw,&d->s0,&d->s1)||
       !texture_axis(rows,h,step_y,ly,th,&d->t0,&d->t1))return 0;
    d->coverage.psm=psm;d->origin_x=lx;d->origin_y=ly;d->tw=tw;d->th=th;d->columns=w;d->rows=h;
    memcpy(d->column,columns,w*sizeof(*columns));memcpy(d->row,rows,h*sizeof(*rows));return 1;
}
static uint32_t texture_tile(uint32_t w,uint32_t x,uint32_t y)
{return ((y/4u)*(w/4u)+x/4u)*64u+(y&3u)*8u+(x&3u)*2u;}
uint32_t gs_gx_pack_texture_profile_impl(void *out,uint32_t capacity,const gs_gx_texture_draw *d,gs_gx_texel_fn sample){
    if(!out||!d||!sample||!d->tw||!d->th||d->tw>1024u||d->th>512u||
       (d->tw&3u)||(d->th&3u)||capacity<d->tw*d->th*4u)return 0;
    uint8_t *p=out;
    for(uint32_t y=0;y<d->th;y++)for(uint32_t x=0;x<d->tw;x++) {
        uint32_t c=sample(d->origin_x+(int32_t)x,d->origin_y+(int32_t)y),t=texture_tile(d->tw,x,y);
        p[t]=c>>24;p[t+1]=c;p[t+32]=c>>8;p[t+33]=c>>16;
    }
    return d->tw*d->th*4u;
}
uint32_t gs_gx_pack_texture(void *out,uint32_t capacity,const gs_gx_texture_draw *d,gs_gx_texel_fn sample)
{
 unsigned profile_previous=gp_enter(GP_GX_UPLOAD);
 uint32_t result=gs_gx_pack_texture_profile_impl(out,capacity,d,sample);
 gp_leave(profile_previous);
 return result;
}

static uint32_t raw_z(const uint8_t *vram,uint32_t off,uint32_t psm)
{
    unsigned n=(psm==2u||psm==10u)?2u:(psm==1u?3u:4u);uint32_t value=0;
    for(unsigned k=0;k<n;k++)value|=(uint32_t)vram[off+k]<<(8*k);return value;
}
/* GS Z16/Z24 compare their native width, not the upper discarded bits of
 * the EE's interpolated 32-bit Z. Z16S uses the same 16-bit comparison range.
 * Keep this check in the CPU-GX import path as well as the main rasterizer. */
uint32_t gs_gx_depth_normalize(uint32_t z,uint32_t psm)
{
    /* PS2 raster Z is SATURATED to the destination width, not wrapped. */
    uint32_t maximum=(psm==2u||psm==10u)?0xffffu:(psm==1u?0xffffffu:UINT32_MAX);
    return z>maximum?maximum:z;
}
static int depth_pass(const gs_gx_pipeline *p,uint32_t stored)
{
    uint32_t incoming=gs_gx_depth_normalize(p->z,p->zpsm);
    return !p->ztest||p->ztst==1u||(p->ztst==2u?incoming>=stored:p->ztst==3u?incoming>stored:0);
}
uint32_t gs_gx_framebuffer_store(uint32_t source,uint32_t dest,
                                  uint32_t mask,uint32_t fba,uint32_t psm)
{
    if(fba && psm==0u)source|=0x80000000u;
    if((psm==2u||psm==10u)) { /* CT16/CT16S bit 15 is the alpha MSB. */
        if(fba)source|=0x8000u;
        return (source&~mask&0xffffu)|(dest&mask&0xffffu);
    }
    if(psm==1u)mask|=0xff000000u; /* PSMCT24 never writes alpha */
    return (source&~mask)|(dest&mask);
}
/* GS TEST_1/2: source alpha comparison. The destination alpha test is
 * evaluated separately and rejects *both* colour and Z writes. */
static int gx_alpha_test_pass(const gs_gx_pipeline *p,unsigned a)
{
    if(!p->ate)return 1;
    unsigned ref=p->aref&255u;
    switch(p->atst&7u){
    case 0: return 0; /* NEVER */
    case 1: return 1; /* ALWAYS */
    case 2: return a<ref;
    case 3: return a<=ref;
    case 4: return a==ref;
    case 5: return a>=ref;
    case 6: return a>ref;
    default:return a!=ref;
    }
}
static int gx_date_pass(const gs_gx_pipeline *p,uint32_t dst,uint32_t psm)
{
    if(!p->date)return 1;
    /* PCSX2's PS2-hardware-tested rule: DATE on CT24 rejects all pixels.
     * CT16 is outside the bounded RGBA import route. */
    if(psm==1u)return 0;
    return ((dst>>31)&1u)==(p->datm&1u);
}
static int blend_div128(int n){return n>=0?n/128:-((-n+127)/128);}
static uint32_t blend_rgb(uint32_t src,uint32_t dst,const gs_gx_pipeline *p,uint32_t psm)
{
    if(!p->blend||(p->pabe&&!(src&0x80000000u)))return src;
    unsigned coeff=p->c==0u?src>>24:p->c==1u?(psm==1u?128u:dst>>24):p->fix;
    uint32_t out=src&0xff000000u;
    for(unsigned k=0;k<3;k++) {
        int s=(src>>(k*8))&255,d=(dst>>(k*8))&255;
        int A=p->a==0u?s:p->a==1u?d:0,B=p->b==0u?s:p->b==1u?d:0,D=p->d==0u?s:p->d==1u?d:0;
        int v=blend_div128((A-B)*(int)coeff)+D;
        if(p->colclamp){if(v<0)v=0;if(v>255)v=255;}
        out|=(uint32_t)(v&255)<<(k*8);
    }
    return out;
}
int gs_gx_import_texture(uint8_t *vram,uint32_t size,const void *pixels,uint32_t capacity,
    const void *source,uint32_t source_capacity,const gs_gx_texture_draw *d)
{
    if(!vram||!pixels||!d||!source||!d->columns||!d->rows||d->columns>640u||d->rows>512u||
       !d->tw||!d->th||d->tw>1024u||d->th>512u||(d->tw&3u)||(d->th&3u)||
       d->columns>d->coverage.width||d->rows>d->coverage.height||
       source_capacity<d->tw*d->th*4u||d->coverage.psm>1u||
       !gs_gx_target_valid(size,d->coverage.bp,d->coverage.bw,d->coverage.x,d->coverage.y,d->coverage.width,d->coverage.height)||
       capacity<d->coverage.width*d->coverage.height*4u)return 0;
    for(uint32_t x=0;x<d->columns;x++)if((uint32_t)(d->column[x]-d->origin_x)>=d->tw)return 0;
    for(uint32_t y=0;y<d->rows;y++)if((uint32_t)(d->row[y]-d->origin_y)>=d->th)return 0;
    for(uint32_t y=0;y<d->coverage.height;y++) {
        if(d->coverage.left[y]>d->coverage.right[y]||d->coverage.right[y]>d->columns||
           (d->coverage.right[y]&&y>=d->rows))return 0;
        if(d->pipeline.ztest||d->pipeline.zwrite)for(uint32_t x=d->coverage.left[y];x<d->coverage.right[y];x++)
            if(gs_mem_z_offset(d->pipeline.zbp,d->coverage.bw,d->coverage.x+x,d->coverage.y+y,d->pipeline.zpsm)==UINT32_MAX)return 0;
    }
    const uint8_t *p=pixels,*tex=source;const gs_gx_pipeline *pipe=&d->pipeline;
    for(uint32_t y=0;y<d->rows;y++)for(uint32_t x=d->coverage.left[y];x<d->coverage.right[y];x++) {
        uint32_t zo=0;
        if(pipe->ztest||pipe->zwrite) {
            zo=gs_mem_z_offset(pipe->zbp,d->coverage.bw,d->coverage.x+x,d->coverage.y+y,pipe->zpsm);
            if(!depth_pass(pipe,raw_z(vram,zo,pipe->zpsm)))continue;
        }
        uint32_t t=texture_tile(d->coverage.width,x,y),s=texture_tile(d->tw,d->column[x]-d->origin_x,d->row[y]-d->origin_y);
        uint32_t c=(uint32_t)p[t+1]|((uint32_t)p[t+32]<<8)|((uint32_t)p[t+33]<<16)|((uint32_t)tex[s]<<24);
        uint32_t off=(uint32_t)target_offset(d->coverage.bp,d->coverage.bw,d->coverage.x+x,d->coverage.y+y);
        uint32_t dst=(uint32_t)vram[off]|((uint32_t)vram[off+1]<<8)|((uint32_t)vram[off+2]<<16)|((uint32_t)vram[off+3]<<24);
        /* Import is a late GS write, not a raw RGBA image copy: DATE
         * happens before ATE and rejects Z as well as colour writes.
         * AFAIL changes the permitted destinations independently. */
        if(!gx_date_pass(pipe,dst,d->coverage.psm))continue;
        int alpha_failed=!gx_alpha_test_pass(pipe,c>>24);
        unsigned afail=pipe->afail&3u;
        int write_color=!(alpha_failed&&(afail==0u||afail==2u));
        int write_z=pipe->zwrite&&!(alpha_failed&&(afail==0u||afail==1u||afail==3u));
        if(write_color){
            if(!d->hardware_blend)c=blend_rgb(c,dst,pipe,d->coverage.psm);
            /* RGB_ONLY is an alpha write mask, applied after FBA. */
            uint32_t mask=pipe->fbmask;
            if(alpha_failed&&afail==3u&&d->coverage.psm==0u)
                mask|=0xff000000u;
            c=gs_gx_framebuffer_store(c,dst,mask,pipe->fba,d->coverage.psm);
            vram[off]=c;vram[off+1]=c>>8;vram[off+2]=c>>16;
            if(!d->coverage.psm)vram[off+3]=c>>24;
        }
        if(write_z) {
            unsigned bytes=pipe->zpsm==1u?3u:(pipe->zpsm==2u||pipe->zpsm==10u)?2u:4u;
            uint32_t bounded_z=gs_gx_depth_normalize(pipe->z,pipe->zpsm);
            for(unsigned k=0;k<bytes;k++)vram[zo+k]=(uint8_t)(bounded_z>>(k*8));
        }
    }
    return 1;
}
int gs_gx_draw_flat(uint32_t kind,uint32_t bp,uint32_t bw,int32_t minx,int32_t miny,
                    int32_t maxx,int32_t maxy,const int32_t *xy,uint32_t rgba,uint32_t scanmsk)
{return gs_gx_draw_flat_psm(0,kind,bp,bw,minx,miny,maxx,maxy,xy,rgba,scanmsk);}
static uint64_t shaded_triangle_counts[3];
uint64_t gs_gx_shaded_triangle_stat(unsigned n){return n<3u?shaded_triangle_counts[n]:0;}
#ifndef GEKKO
int gs_gx_draw_flat_pipeline(uint32_t psm,uint32_t kind,uint32_t bp,uint32_t bw,int32_t minx,int32_t miny,int32_t maxx,int32_t maxy,const int32_t *xy,uint32_t rgba,uint32_t scanmsk,const gs_gx_pipeline *pipeline)
{(void)psm;(void)kind;(void)bp;(void)bw;(void)minx;(void)miny;(void)maxx;(void)maxy;(void)xy;(void)rgba;(void)scanmsk;(void)pipeline;return 0;}
int gs_gx_draw_gouraud_triangle(uint32_t psm,uint32_t bp,uint32_t bw,int32_t minx,int32_t miny,int32_t maxx,int32_t maxy,const int32_t *xy,const uint32_t *rgba,uint32_t scanmsk,const gs_gx_pipeline *pipeline)
{(void)psm;(void)bp;(void)bw;(void)minx;(void)miny;(void)maxx;(void)maxy;(void)xy;(void)rgba;(void)scanmsk;(void)pipeline;return 0;}
int gs_gx_draw_uv_decal_triangle(uint32_t psm,uint32_t bp,uint32_t bw,int32_t minx,int32_t miny,int32_t maxx,int32_t maxy,const int32_t *xy,const float *uv,uint32_t tex_w,uint32_t tex_h,uint32_t alpha,uint32_t scanmsk,gs_gx_texel_fn sample)
{(void)psm;(void)bp;(void)bw;(void)minx;(void)miny;(void)maxx;(void)maxy;(void)xy;(void)uv;(void)tex_w;(void)tex_h;(void)alpha;(void)scanmsk;(void)sample;return 0;}
int gs_gx_draw_texture_sprite(uint32_t psm,uint32_t bp,uint32_t bw,int32_t x,int32_t y,uint32_t w,uint32_t h,const int32_t *columns,const int32_t *rows,double step_x,double step_y,uint32_t scanmsk,gs_gx_texel_fn sample,const gs_gx_pipeline *pipeline)
{(void)pipeline;(void)psm;(void)bp;(void)bw;(void)x;(void)y;(void)w;(void)h;(void)columns;(void)rows;(void)step_x;(void)step_y;(void)scanmsk;(void)sample;return 0;}
int gs_gx_ready(void){return 0;}
int gs_gx_render_active(void){return 0;}
uint64_t gs_gx_work_count(unsigned index){(void)index;return 0;}
uint64_t gs_gx_sync_count(unsigned index){(void)index;return 0;}
uint64_t gs_gx_watchdog_count(unsigned index){(void)index;return 0;}
int gs_gx_draw_native_affine_triangle(uint32_t psm,uint32_t bp,uint32_t bw,
 int32_t minx,int32_t miny,int32_t maxx,int32_t maxy,const int32_t *xy,
 const float *uv,uint32_t tex_w,uint32_t tex_h,uint32_t ws,uint32_t wt,
 uint32_t linear,uint32_t scanmsk,gs_gx_texel_fn sample,const gs_gx_pipeline *pipe)
{(void)psm;(void)bp;(void)bw;(void)minx;(void)miny;(void)maxx;(void)maxy;(void)xy;(void)uv;
 (void)tex_w;(void)tex_h;(void)ws;(void)wt;(void)linear;(void)scanmsk;(void)sample;(void)pipe;return 0;}
int gs_gx_draw_native_stq_triangle(uint32_t psm,uint32_t bp,uint32_t bw,
 int32_t minx,int32_t miny,int32_t maxx,int32_t maxy,const int32_t *xy,
 const float *uv,const float *q,const uint32_t *rgba,uint32_t tw,uint32_t th,
 uint32_t ws,uint32_t wt,uint32_t linear,uint32_t scanmsk,
 gs_gx_texel_fn sample,const gs_gx_pipeline *p,uint32_t conservative_z,const gs_gx_native_color *color)
{(void)color;(void)conservative_z;(void)psm;(void)bp;(void)bw;(void)minx;(void)miny;(void)maxx;(void)maxy;(void)xy;(void)uv;(void)q;(void)rgba;(void)tw;(void)th;(void)ws;(void)wt;(void)linear;(void)scanmsk;(void)sample;(void)p;return 0;}

uint64_t gs_gx_native_affine_stat(unsigned n){(void)n;return 0;}
int gs_gx_draw_shaded_triangle(uint32_t psm,uint32_t bp,uint32_t bw,int32_t minx,int32_t miny,int32_t maxx,int32_t maxy,const int32_t *xy,uint32_t scanmsk,gs_gx_texel_fn sample,const gs_gx_pipeline *pipeline)
{(void)psm;(void)bp;(void)bw;(void)minx;(void)miny;(void)maxx;(void)maxy;(void)xy;(void)scanmsk;(void)sample;(void)pipeline;return 0;}
int gs_gx_draw_mapped_triangle(uint32_t psm,uint32_t bp,uint32_t bw,int32_t x,int32_t y,uint32_t w,uint32_t h,
 const int32_t *xy,const int32_t *columns,const int32_t *rows,double du,double dv,uint32_t scanmsk,
 gs_gx_texel_fn sample,const gs_gx_pipeline *pipeline)
{(void)psm;(void)bp;(void)bw;(void)x;(void)y;(void)w;(void)h;(void)xy;(void)columns;(void)rows;(void)du;(void)dv;(void)scanmsk;(void)sample;(void)pipeline;return 0;}
uint64_t gs_gx_texture_count(unsigned n){(void)n;return 0;}
uint64_t gs_gx_pipeline_count(unsigned n){(void)n;return 0;}
uint64_t gs_gx_surface_count(unsigned n){(void)n;return 0;}
uint64_t gs_gx_resident_pipeline_count(unsigned n){(void)n;return 0;}
int gs_gx_set_residency_enabled(int enabled){(void)enabled;return 1;}
void gs_gx_source_key(uint32_t lo,uint32_t hi,int valid){(void)lo;(void)hi;(void)valid;}
uint64_t gs_gx_source_cache_count(unsigned n){(void)n;return 0;}
int gs_gx_draw_flat_psm(uint32_t psm,uint32_t kind,uint32_t bp,uint32_t bw,int32_t minx,int32_t miny,
                    int32_t maxx,int32_t maxy,const int32_t *xy,uint32_t rgba,uint32_t scanmsk)
{(void)psm;(void)kind;(void)bp;(void)bw;(void)minx;(void)miny;(void)maxx;(void)maxy;(void)xy;(void)rgba;(void)scanmsk;return 0;}
#endif
#ifdef GEKKO
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define GX_PRESENT_FIFO (256u*1024u)
#define GX_PRESENT_TEXTURE (1024u*512u*4u)
static void *fifo,*texture;
static int initialized;
/* R1330-I: retain CPU fences only at actual memory ownership transfers.
 * GPU copy -> GPU sample is ordered inside the FIFO with PixModeSync. */
static int texture_in_flight;
static uint64_t sync_counts[8];
uint64_t gs_gx_sync_count(unsigned n){return n<8u?sync_counts[n]:0;}
/* R1389-WD: GX_DrawDone() sleeps until the PE-finish interrupt wakes it. A lost
 * interrupt or stalled GP therefore froze the whole console with no log line.
 * Same command (DrawDone) and same interrupt, but the CPU polls a flag with a
 * time limit. On timeout the GX path is switched off so the console keeps
 * running and the log shows where. Untested on hardware. */
#define GX_WAIT_LIMIT_TB ((60750000u*4u)/10u)
#define GX_LOST_IRQ_GRACE_TB (60750000u/20u)
static volatile int gx_done_flag;
static uint32_t gx_diag[8];static int gx_diag_taken;
static uint64_t gx_wd[8]; /* timeouts, last reason, last gpu_pending, max wait tb, bounded waits, callbacks */
uint64_t gs_gx_watchdog_count(unsigned n){return n<8u?gx_wd[n]:0;}
static void gx_done_callback(void){gx_done_flag=1;gx_wd[5]++;}
static void gx_vtx_report(unsigned endline)
{
 if(!gx_in_begin)return;
 gx_in_begin=0;
 if(gx_vcnt==gx_vexp)return;
 gx_vmis[0]++;
 if(gx_vmis[0]>32u)return;
 FILE *f=fopen("sd:/pcsx2/logs/" GEKKO2_RELEASE "-gx-timeout.txt","ab");
 if(!f)return;
 fprintf(f,"GX_VTX_MISMATCH begin_line=%u end_line=%u declared=%u written=%u total=%llu\n",gx_vline,endline,gx_vexp,gx_vcnt,(unsigned long long)gx_vmis[0]);
 fflush(f);fclose(f);
}
static void gx_wait_timeout_marker(unsigned reason)
{
 FILE *f=fopen("sd:/pcsx2/logs/" GEKKO2_RELEASE "-gx-timeout.txt","ab");
 if(!f)return;
 fprintf(f,"GX_LOST_IRQ_DIAG pe_isr=%04x pi_intsr=%08x pi_intmr=%08x ovh=%u und=%u rdi=%u cmi=%u brk=%u tex_in_flight=%d\n",
  gx_diag[0],gx_diag[1],gx_diag[2],gx_diag[3],gx_diag[4],gx_diag[5],gx_diag[6],gx_diag[7],texture_in_flight);
 fprintf(f,"GX_WAIT_TIMEOUT reason=%u gpu_pending=%d limit_tb=%u timeouts=%llu waits=%llu\n",
  reason,(int)gs_mem_gpu_pending(),GX_WAIT_LIMIT_TB,(unsigned long long)gx_wd[0],(unsigned long long)gx_wd[4]);
 fprintf(f,"GX_WD11_CTX begins_before_wait=%u last_begin_line=%u recoveries=%llu\n",gx_ctx_begins,gx_last_line,(unsigned long long)gx_wd[7]);
 fflush(f);fclose(f);
}
static void gx_bounded_draw_done(unsigned reason)
{
 gx_done_flag=0;gx_diag_taken=0;gx_ctx_begins=gx_begins_since_wait;gx_begins_since_wait=0;
 GX_SetDrawDoneCallback(gx_done_callback);
 GX_SetDrawDone();
 uint32_t begin,now;__asm__ volatile("mftb %0":"=r"(begin));
 gx_wd[4]++;
 while(!gx_done_flag) {
  __asm__ volatile("mftb %0":"=r"(now));
  /* R1389-WD7: WD6 timeout file showed the GP idle (readidle=1 cmdidle=1, FIFO
   * rd==wr, no overflow) while the finish callback never arrived: the finish
   * interrupt was lost, not the GP stalled. After a 50 ms grace period an idle
   * GP with a drained FIFO counts as finished (the DrawDone token is the last
   * command in the FIFO), so rendering continues instead of switching GX off.
   * Untested on hardware. */
  if((uint32_t)(now-begin)>GX_LOST_IRQ_GRACE_TB) {
   /* Only plain register reads here: GX_GetCPUFifo()/__GX_SaveFifo() blocked
    * the console in WD7 (hang-pc report: WAITING inside GX_GetCPUFifo). */
   u8 ovh=0,und=0,rdi=0,cmi=0,brk=0;
   GX_GetGPStatus(&ovh,&und,&rdi,&cmi,&brk);
   uint16_t pe=*(volatile uint16_t*)0xCC00100Au;
   uint32_t pis=*(volatile uint32_t*)0xCC003000u,pim=*(volatile uint32_t*)0xCC003004u;
   if(!gx_diag_taken){gx_diag[0]=pe;gx_diag[1]=pis;gx_diag[2]=pim;gx_diag[3]=ovh;gx_diag[4]=und;gx_diag[5]=rdi;gx_diag[6]=cmi;gx_diag[7]=brk;gx_diag_taken=1;}
   if(rdi&&cmi&&((uint32_t)(now-begin)>GX_LOST_IRQ_GRACE_TB*2u)){gx_wd[6]++;gx_done_flag=1;gx_wait_timeout_marker(100u+reason);break;}
  }
  if((uint32_t)(now-begin)>GX_WAIT_LIMIT_TB) {
   gx_wd[0]++;gx_wd[1]=reason;gx_wd[2]=(uint64_t)(unsigned)gs_mem_gpu_pending();
   gx_wait_timeout_marker(200u+reason);
   /* R1389-WD10 beta: try GX_AbortFrame() (libogc's documented GP reset) up to
    * twice before giving up. Not tested on hardware. */
   if(gx_wd[7]<8u) {
    gx_wd[7]++;
    GX_AbortFrame();
    gx_done_flag=0;GX_SetDrawDoneCallback(gx_done_callback);GX_SetDrawDone();
    uint32_t b2,n2;__asm__ volatile("mftb %0":"=r"(b2));
    do{__asm__ volatile("mftb %0":"=r"(n2));}while(!gx_done_flag&&(uint32_t)(n2-b2)<60750000u);
    if(gx_done_flag){gx_wait_timeout_marker(300u+reason);return;}
   }
   gx_wait_failed=1;render_enabled=0;
#ifdef GEKKO2_GX_DUMP
   gs_gx_dump_record("WAIT_TIMEOUT",reason,(uint32_t)gs_mem_gpu_pending(),0,0);gs_gx_dump_flush(NULL);
#endif
   gx_wait_timeout_marker(reason);
   return;
  }
 }
 __asm__ volatile("mftb %0":"=r"(now));
 if((uint32_t)(now-begin)>gx_wd[3])gx_wd[3]=(uint32_t)(now-begin);
}
static void gx_cpu_wait_profile_impl(unsigned reason){
#ifdef GEKKO2_GX_DUMP
 gs_gx_dump_record("WAIT_ENTER",reason,gs_mem_gpu_pending(),0,0);GXD_FLUSH();
#endif
 uint32_t begin,end;__asm__ volatile("mftb %0":"=r"(begin));
 if(!gx_wait_failed)gx_bounded_draw_done(reason);
 __asm__ volatile("mftb %0":"=r"(end));
 sync_counts[reason]++;sync_counts[4u+reason]+=(uint32_t)(end-begin);
 texture_in_flight=0;
#ifdef GEKKO2_GX_DUMP
 gs_gx_dump_record("WAIT_DONE",reason,gs_mem_gpu_pending(),end-begin,0);GXD_FLUSH();
#endif
}
static void gx_cpu_wait(unsigned reason)
{
 unsigned profile_previous=gp_enter(GP_GX_WAIT);
 gx_cpu_wait_profile_impl(reason);
 gp_leave(profile_previous);
}

int gs_gx_ready(void){return initialized;}
int gs_gx_render_active(void){return initialized&&render_enabled;}
static uint32_t efb_width,efb_height;
static void *readback;
static gs_gx_flat_draw flat_draw;
static int capture_flat;
static int capture_texture;
static gs_gx_surface *surface;
static void *surface_pixels;
static int surface_texture_valid;
static uint64_t surface_counts[11];
static uint8_t surface_seeded[GS_GX_SURFACE_PIXELS/16u];
static uint64_t resident_pipeline_counts[11];
uint64_t gs_gx_resident_pipeline_count(unsigned n){return n<11u?resident_pipeline_counts[n]:0;}
static uint32_t source_key_lo,source_key_hi,source_key_valid;
static struct {uint32_t lo,hi,valid;int32_t x,y;uint32_t w,h;gs_gx_texel_fn sample;} source_cache;
static uint64_t source_cache_counts[5]; /* [4] no-write GX texture-cache invalidations avoided */
static unsigned source_upload_pending;
static gs_gx_texel_fn additive_source;
static uint32_t additive_fix;
static uint32_t additive_sample(int32_t x,int32_t y)
{return gs_gx_additive_pixel(additive_source(x,y),additive_fix);}
void gs_gx_source_key(uint32_t lo,uint32_t hi,int valid){source_key_lo=lo;source_key_hi=hi;source_key_valid=!!valid;}
uint64_t gs_gx_source_cache_count(unsigned n){return n<5u?source_cache_counts[n]:0;}
static uint32_t source_pack(gs_gx_texture_draw *d,gs_gx_texel_fn sample)
{
 uint32_t lo=source_key_lo,hi=source_key_hi,valid=source_key_valid;source_key_valid=0;
 if(gekko2_opt_enabled(GEKKO2_OPT_GX_SOURCE_CACHE)&&valid&&source_cache.valid&&lo==source_cache.lo&&hi==source_cache.hi&&sample==source_cache.sample&&
    d->origin_x==source_cache.x&&d->origin_y==source_cache.y&&d->tw==source_cache.w&&d->th==source_cache.h) {
  source_cache_counts[0]++;return d->tw*d->th*4u;
 }
 if(texture_in_flight)gx_cpu_wait(0); /* CPU overwrites a sampled buffer. */
 uint32_t bytes=gs_gx_pack_texture(texture,GX_PRESENT_TEXTURE,d,sample);source_cache_counts[1]++;
 if(bytes)source_upload_pending=1u;
 source_cache.valid=valid&&bytes;source_cache.lo=lo;source_cache.hi=hi;source_cache.x=d->origin_x;source_cache.y=d->origin_y;
 source_cache.w=d->tw;source_cache.h=d->th;source_cache.sample=sample;source_cache_counts[2]+=bytes;return bytes;
}
static __attribute__((noinline)) void source_upload(uint32_t bytes)
{
 /* Cache hits preserve the same bytes in the same allocation. Keep a
  * pending upload across rejected draws so the first successful consumer
  * still flushes newly packed CPU data. GX texture invalidation remains. */
 /* A decoded source-cache hit neither writes the aligned texture buffer nor
  * changes its GX-visible bytes. Only invalidate GX texture cache after
  * actually flushing a newly packed source. Never skip if a rejected draw
  * left an upload pending: that source still needs the first invalidation. */
 if(source_upload_pending){DCFlushRange(texture,bytes);GX_InvalidateTexAll();source_upload_pending=0u;}
 else {source_cache_counts[3]+=bytes;source_cache_counts[4]++;}
}
uint64_t gs_gx_surface_count(unsigned n){return n<11u?surface_counts[n]:0;}
static int surface_present(void *xfb,GXRModeObj *mode,uint32_t bp,uint32_t bw,uint32_t sx,uint32_t sy,uint32_t width,uint32_t height);
static int surface_draw_flat(const gs_gx_flat_draw *draw);
static void pipeline_shader_target(const gs_gx_texture_draw *d,void *pixels,uint32_t w,uint32_t h);
static gs_gx_texture_draw texture_draw;
static uint32_t geometry_kind,geometry_rgba;
static int32_t geometry_xy[6];
static uint64_t texture_counts[4],pipeline_counts[6];
uint64_t gs_gx_pipeline_count(unsigned n){return n<6u?pipeline_counts[n]:0;}
static void *depth_texture,*destination_texture;
static uint32_t depth_capacity,destination_capacity;
static int grow_texture(void **p,uint32_t *capacity,uint32_t needed)
{
    if(*capacity>=needed)return 1;
    void *next=memalign(32,needed);if(!next)return 0;
    free(*p);*p=next;*capacity=needed;return 1;
}
static uint8_t zzero[64] __attribute__((aligned(32)));
uint64_t gs_gx_texture_count(unsigned n){return n<4u?texture_counts[n]:0;}
static uint64_t work_counts[5];
uint64_t gs_gx_work_count(unsigned index){return index<5u?work_counts[index]:0;}
static struct {uint32_t bp,bw,x,y,width,height,alpha,bytes;} capture;
static int resolve_capture_profile_impl(void *opaque,uint8_t *vram,uint32_t size){
    (void)opaque;
    work_counts[3]++;
    gx_cpu_wait(1); /* GPU copy must finish before invalidating CPU cache. */
    DCInvalidateRange(readback,capture.bytes);
    if(capture_texture)return gs_gx_import_texture(vram,size,readback,capture.bytes,texture,texture_draw.tw*texture_draw.th*4u,&texture_draw);
    if(capture_flat)return gs_gx_import_flat(vram,size,readback,capture.bytes,&flat_draw);
    return gs_gx_unpack_psmct32(vram,size,readback,capture.bytes,capture.bp,capture.bw,
                               capture.x,capture.y,capture.width,capture.height,capture.alpha);
}
static int resolve_capture(void *opaque,uint8_t *vram,uint32_t size)
{
 unsigned profile_previous=gp_enter(GP_GX_READBACK);
 int result=resolve_capture_profile_impl(opaque,vram,size);
 gp_leave(profile_previous);
 return result;
}

int gs_gx_capture_vram_psmct32(uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,
                             uint32_t width,uint32_t height,uint32_t alpha)
{
    if(!initialized||width>efb_width||height>efb_height||alpha>255u||!gs_gx_target_valid(GS_MEM_SIZE,bp,bw,x,y,width,height)||
       !gs_mem_sync())return 0;
    if(!readback)readback=memalign(32,GX_PRESENT_TEXTURE);
    if(!readback||!gs_mem_gpu_bind(resolve_capture,NULL))return 0;
    capture_flat=0;capture_texture=0;
    capture.bp=bp;capture.bw=bw;capture.x=x;capture.y=y;
    capture.width=width;capture.height=height;capture.alpha=alpha;capture.bytes=width*height*4u;
    DCFlushRange(readback,capture.bytes);
    /* Disable presentation vertical filtering for exact texture readback. */
    static uint8_t samples[12][2],filter[7];
    GX_SetCopyFilter(GX_FALSE,samples,GX_FALSE,filter);
    GX_SetTexCopySrc(0,0,width,height);
    GX_SetTexCopyDst(width,height,GX_TF_RGBA8,GX_FALSE);
    GX_CopyTex(readback,GX_FALSE);
    return gs_mem_gpu_mark_pending();
}
static void vertex(float x,float y,float s,float t);
static int surface_enabled=1;
int gs_gx_set_residency_enabled(int enabled)
{if(!gs_mem_sync())return 0;surface_enabled=!!enabled;return 1;}
static void color_vertex(float x,float y,uint32_t rgba);
static void surface_state(uint32_t w,uint32_t h,int textured)
{
 GX_SetViewport(0,0,w,h,0,1);GX_SetScissor(0,0,w,h);
 GX_SetPixelFmt(GX_PF_RGB8_Z24,GX_ZC_LINEAR);GX_SetDither(GX_FALSE);GX_SetFieldMode(GX_FALSE,GX_FALSE);
 GX_SetCullMode(GX_CULL_NONE);GX_SetZMode(GX_FALSE,GX_ALWAYS,GX_FALSE);GX_SetZTexture(GX_ZT_DISABLE,GX_TF_Z24X8,0);
 GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);GX_SetColorUpdate(GX_TRUE);GX_SetAlphaUpdate(GX_FALSE);
 GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
 GX_SetNumChans(textured?0:1);GX_SetNumTexGens(textured?1:0);GX_SetNumTevStages(1);
 GX_ClearVtxDesc();GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);
 if(textured) {
  GX_SetVtxDesc(GX_VA_TEX0,GX_DIRECT);GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_TEX0,GX_TEX_ST,GX_F32,0);
  GX_SetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX2x4,GX_TG_TEX0,GX_IDENTITY);
  GX_SetTevOrder(0,GX_TEXCOORD0,GX_TEXMAP0,GX_COLORNULL);GX_SetTevOp(0,GX_REPLACE);
 } else {
  GX_SetVtxDesc(GX_VA_CLR0,GX_DIRECT);GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_CLR0,GX_CLR_RGBA,GX_RGBA8,0);
  GX_SetChanCtrl(GX_COLOR0A0,GX_DISABLE,GX_SRC_REG,GX_SRC_VTX,GX_LIGHTNULL,GX_DF_NONE,GX_AF_NONE);
  GX_SetTevOrder(0,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLOR0A0);GX_SetTevOp(0,GX_PASSCLR);
 }
 Mtx model;Mtx44 projection;guMtxIdentity(model);guOrtho(projection,0,h,0,w,-1,1);
 GX_LoadPosMtxImm(model,GX_PNMTX0);GX_SetCurrentMtx(GX_PNMTX0);GX_LoadProjectionMtx(projection,GX_ORTHOGRAPHIC);
}
static void surface_texture(void *pixels,uint32_t w,uint32_t h,int filter)
{
 GXTexObj object;GX_InitTexObj(&object,pixels,w,h,GX_TF_RGBA8,GX_CLAMP,GX_CLAMP,GX_FALSE);
 GX_InitTexObjLOD(&object,filter,filter,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);GX_LoadTexObj(&object,GX_TEXMAP0);
}
static void surface_quad(uint32_t w,uint32_t h)
{
 GXB(GX_QUADS,GX_VTXFMT0,4);vertex(0,0,0,0);vertex(w,0,1,0);vertex(w,h,1,1);vertex(0,h,0,1);GX_End();
}
static void surface_copy(void *pixels,uint32_t x,uint32_t y,uint32_t w,uint32_t h)
{
#ifdef GEKKO2_GX_DUMP
 gs_gx_dump_record("COPY_RECT",x,y,w,h);
 gs_gx_dump_record("COPY_ENTER",(uint32_t)pixels,w*h*4u,0,0);GXD_FLUSH();
#endif
 static uint8_t samples[12][2],filter[7];
 /* R1389-WD11 beta: drain pending draws into the pixel engine before the EFB copy. */
 GX_Flush();GX_PixModeSync();
 GX_SetCopyFilter(GX_FALSE,samples,GX_FALSE,filter);GX_SetTexCopySrc(x,y,w,h);
 GX_SetTexCopyDst(w,h,GX_TF_RGBA8,GX_FALSE);GX_CopyTex(pixels,GX_FALSE);
 GX_PixModeSync();GX_InvalidateTexAll();
#ifdef GEKKO2_GX_DUMP
 gs_gx_dump_record("COPY_DONE",(uint32_t)pixels,w*h*4u,0,0);GXD_FLUSH();
#endif
}
static int surface_snapshot(void)
{
 if(!surface||!surface->active||!surface_pixels)return 0;
 if(!surface_texture_valid){surface_copy(surface_pixels,0,0,surface->width,surface->height);surface_texture_valid=1;surface_counts[3]++;}
 return 1;
}
static int resolve_surface_profile_impl(void *opaque,uint8_t *vram,uint32_t size){
 (void)opaque;
 if(!surface||!surface->active)return 0;
 /* A seed has no guest-visible dirty pixels. Still drain its queued read
  * before returning CPU ownership of the seed allocation. */
 if(!surface->dirty_pixels){gx_cpu_wait(1);surface->active=0;surface_counts[8]++;return 1;}
 uint32_t x=surface->dirty_x0&~3u,y=surface->dirty_y0&~3u;
 uint32_t w=((surface->dirty_x1+3u)&~3u)-x,h=((surface->dirty_y1+3u)&~3u)-y;
 uint32_t bytes=w*h*4u;
 if(!readback)readback=memalign(32,GX_PRESENT_TEXTURE);
 if(!readback)return 0;
 /* A distinct destination never overwrites a seed texture that GX may
  * still sample. Copy only the aligned dirty bounding rectangle. */
 surface_copy(readback,x,y,w,h);gx_cpu_wait(1);
 DCInvalidateRange(readback,bytes);
 if(!gs_gx_surface_import_rect(surface,vram,size,readback,bytes,x,y,w,h))return 0;
 surface_texture_valid=0;
 surface_counts[7]+=(uint64_t)surface->width*surface->height*4u-bytes;
 surface_counts[2]++;work_counts[3]++;work_counts[2]+=bytes;return 1;
}
static int resolve_surface(void *opaque,uint8_t *vram,uint32_t size)
{
 unsigned profile_previous=gp_enter(GP_GX_READBACK);
 int result=resolve_surface_profile_impl(opaque,vram,size);
 gp_leave(profile_previous);
 return result;
}

static int surface_ensure_height(const gs_gx_flat_draw *draw,uint32_t h)
{
 if(h>512u||h>efb_height||(h&3u))return 0;
 if(!surface_enabled||!h||draw->bw>efb_width||draw->bw>640u||draw->x+draw->width>draw->bw||
    draw->y+draw->height>h||!gs_gx_target_valid(GS_MEM_SIZE,draw->bp,draw->bw,0,0,draw->bw,h))return 0;
 if(surface&&surface->active&&(surface->bp!=draw->bp||surface->bw!=draw->bw||surface->height!=h)) {
  if(!gs_mem_sync())return 0;
 }
 if(!surface||!surface->active) {
  if(!gs_mem_sync())return 0;
  if(!surface)surface=memalign(32,sizeof(*surface));
  if(!surface_pixels)surface_pixels=memalign(32,GX_PRESENT_TEXTURE);
  if(!surface||!surface_pixels)return 0;
  if(!gs_mem_gpu_bind(resolve_surface,NULL)||!gs_gx_surface_begin(surface,draw->bp,draw->bw,draw->bw,h))return 0;
  if(!gs_mem_gpu_protect_range(draw->bp*4u,(uint32_t)target_offset(draw->bp,draw->bw,draw->bw-1,h-1)+4u)){surface->active=0;return 0;}
  /* New target: retire all old allocation consumers before repacking it. */
  if(texture_in_flight)gx_cpu_wait(0);
  memset(surface_seeded,0,sizeof(surface_seeded));
  surface_texture_valid=0;surface_counts[0]++;
 }
 /* Seed only uninitialized 4x4 tiles touched by this draw. A seeded tile
  * may already contain GX output, so never overwrite it from CPU VRAM. */
 uint32_t x0=draw->x&~3u,y0=draw->y&~3u;
 uint32_t x1=(draw->x+draw->width+3u)&~3u,y1=(draw->y+draw->height+3u)&~3u;
 const uint8_t *vram=NULL;uint32_t uploaded=0;
 for(uint32_t y=y0;y<y1;y+=4u)for(uint32_t x=x0;x<x1;) {
  uint32_t index=(y/4u)*(draw->bw/4u)+x/4u;
  if(surface_seeded[index]){surface_counts[10]++;x+=4u;continue;}
  uint32_t end=x+4u;
  while(end<x1&&!surface_seeded[(y/4u)*(draw->bw/4u)+end/4u])end+=4u;
  if(!vram){vram=gs_mem_read_range(0,0);if(!vram)return 0;}
  /* The CPU reads only these new tile bytes. An explicit read per pixel
   * would resolve our own pending target; new tiles are not GPU dirty. */
  uint8_t *pixels=surface_pixels;
  for(uint32_t yy=y;yy<y+4u;yy++)for(uint32_t xx=x;xx<end;xx++) {
   uint32_t o=(uint32_t)target_offset(draw->bp,draw->bw,xx,yy),t=texture_tile(draw->bw,xx,yy);
   pixels[t]=255;pixels[t+1]=vram[o];pixels[t+32]=vram[o+1];pixels[t+33]=vram[o+2];
   surface->alpha[yy*draw->bw+xx]=vram[o+3];
  }
  uint32_t bytes=(end-x)*4u*4u;
  DCFlushRange(pixels+texture_tile(draw->bw,x,y),bytes);GX_InvalidateTexAll();
  surface_state(draw->bw,h,1);surface_texture(surface_pixels,draw->bw,h,GX_NEAR);
  GXB(GX_QUADS,GX_VTXFMT0,4);
  vertex(x,y,(float)x/draw->bw,(float)y/h);vertex(end,y,(float)end/draw->bw,(float)y/h);
  vertex(end,y+4u,(float)end/draw->bw,(float)(y+4u)/h);vertex(x,y+4u,(float)x/draw->bw,(float)(y+4u)/h);GX_End();
  for(uint32_t xx=x;xx<end;xx+=4u)surface_seeded[(y/4u)*(draw->bw/4u)+xx/4u]=1;
  texture_in_flight=1;surface_texture_valid=0;
  if(!gs_mem_gpu_mark_pending()){gx_cpu_wait(0);surface->active=0;return 0;}
  uploaded+=bytes;surface_counts[9]++;x=end;
 }
 surface_counts[5]+=uploaded;

 return 1;
}
static int surface_ensure(const gs_gx_flat_draw *draw)
{return surface_ensure_height(draw,efb_height<512u?efb_height&~3u:512u);}
static int surface_draw_flat(const gs_gx_flat_draw *draw)
{
 if(!surface_ensure(draw))return 0;
 if(!gs_gx_surface_mark(surface,draw))return 0;
 surface_state(surface->width,surface->height,0);
 uint32_t quads=0;
 for(uint32_t y=0;y<draw->height;) {
  uint32_t end=y+1;while(end<draw->height&&draw->left[end]==draw->left[y]&&draw->right[end]==draw->right[y])end++;
  if(draw->right[y]>draw->left[y])quads++;y=end;
 }
 #ifdef GEKKO2_GX_DUMP
 if(quads){gs_gx_dump_record("DRAW_ENTER",quads*4u,gs_mem_gpu_pending(),0,0);GXD_FLUSH();}
#endif
 if(quads)GXB(GX_QUADS,GX_VTXFMT0,quads*4);
 for(uint32_t y=0;y<draw->height;) {
  uint32_t end=y+1,l=draw->left[y]+draw->x,r=draw->right[y]+draw->x;
  while(end<draw->height&&draw->left[end]==draw->left[y]&&draw->right[end]==draw->right[y])end++;
  if(r>l){color_vertex(l,draw->y+y,draw->rgba);color_vertex(r,draw->y+y,draw->rgba);
   color_vertex(r,draw->y+end,draw->rgba);color_vertex(l,draw->y+end,draw->rgba);}y=end;
 }
 if(quads)GX_End();surface_texture_valid=0;
 if(!gs_mem_gpu_mark_pending())return 0;
 surface_counts[1]++;work_counts[0]++;work_counts[1]+=quads;return 1;
}
static int surface_draw_textured(gs_gx_texture_draw *d,uint32_t bytes)
{
 if(!bytes||!surface_ensure(&d->coverage))return 0;
 if(d->hardware_blend==1u) {
  /* A resident blend samples only its aligned destination rectangle.
   * Allocate the maximum once: never free a buffer still sampled by GX. */
  if(destination_capacity<GS_GX_SURFACE_PIXELS*4u && texture_in_flight)gx_cpu_wait(0);
  if(!grow_texture(&destination_texture,&destination_capacity,GS_GX_SURFACE_PIXELS*4u))return 0;
  surface_copy(destination_texture,d->coverage.x,d->coverage.y,d->coverage.width,d->coverage.height);
  resident_pipeline_counts[10]+=(uint64_t)d->coverage.width*d->coverage.height*4u;
 }
 gs_gx_flat_draw alpha_mark=d->coverage;alpha_mark.psm=1;
 if(!gs_gx_surface_mark(surface,&alpha_mark))return 0;
 if(!d->coverage.psm) {
  const uint8_t *source=texture;
  for(uint32_t y=0;y<d->rows;y++)for(uint32_t x=d->coverage.left[y];x<d->coverage.right[y];x++) {
   uint32_t i=(d->coverage.y+y)*surface->width+d->coverage.x+x;
   uint32_t t=texture_tile(d->tw,d->column[x]-d->origin_x,d->row[y]-d->origin_y);
   surface->dirty[i]|=2;surface->alpha[i]=source[t];
  }
 }
 source_upload(bytes);surface_state(surface->width,surface->height,1);
 surface_texture(texture,d->tw,d->th,GX_NEAR);
 uint32_t quads=0;
 for(uint32_t y=0;y<d->coverage.height;) {
  uint32_t end=y+1;while(end<d->coverage.height&&d->coverage.left[end]==d->coverage.left[y]&&d->coverage.right[end]==d->coverage.right[y])end++;
  if(d->coverage.right[y]>d->coverage.left[y])quads++;y=end;
 }
 uint32_t passes=d->hardware_blend==1u&&d->pipeline.pabe?2u:1u;
 for(uint32_t pass=0;pass<passes;pass++) {
  gs_gx_texture_draw shader=*d;
  if(passes==2u&&pass==0u)shader.hardware_blend=0;
  /* Reset the state between complementary PABE passes. */
  surface_state(surface->width,surface->height,1);surface_texture(texture,d->tw,d->th,GX_NEAR);
  if(shader.hardware_blend==2u)GX_SetBlendMode(GX_BM_BLEND,GX_BL_ONE,GX_BL_ONE,GX_LO_COPY);
  else pipeline_shader_target(&shader,destination_texture,d->coverage.width,d->coverage.height);
  if(passes==2u)GX_SetAlphaCompare(pass?GX_GEQUAL:GX_LESS,128,GX_AOP_AND,GX_ALWAYS,0);
  #ifdef GEKKO2_GX_DUMP
 if(quads){gs_gx_dump_record("DRAW_ENTER",quads*4u,gs_mem_gpu_pending(),0,0);GXD_FLUSH();}
#endif
 if(quads)GXB(GX_QUADS,GX_VTXFMT0,quads*4);
  for(uint32_t y=0;y<d->coverage.height;) {
  uint32_t end=y+1,l=d->coverage.left[y],r=d->coverage.right[y];
  while(end<d->coverage.height&&d->coverage.left[end]==l&&d->coverage.right[end]==r)end++;
  if(r>l) {
   float s0=d->s0+(d->s1-d->s0)*(float)l/d->columns,s1=d->s0+(d->s1-d->s0)*(float)r/d->columns;
   float t0=d->t0+(d->t1-d->t0)*(float)y/d->rows,t1=d->t0+(d->t1-d->t0)*(float)end/d->rows;
   float vx[4]={d->coverage.x+l,d->coverage.x+r,d->coverage.x+r,d->coverage.x+l};
   float vy[4]={d->coverage.y+y,d->coverage.y+y,d->coverage.y+end,d->coverage.y+end};
   float ss[4]={s0,s1,s1,s0},tt[4]={t0,t0,t1,t1};
   for(unsigned k=0;k<4;k++) {
    vertex(vx[k],vy[k],ss[k],tt[k]);
    if(shader.hardware_blend==1u)GX_TexCoord2f32((vx[k]-d->coverage.x)/d->coverage.width,(vy[k]-d->coverage.y)/d->coverage.height);
   }
  }y=end;
  }
  if(quads){GX_End();texture_in_flight=1;}
 }
 GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
 surface_texture_valid=0;if(!gs_mem_gpu_mark_pending())return 0;
#ifdef GEKKO2_GX_RESIDENT_FENCE
 /* Diagnostic ownership boundary: finish resident commands before the
  * caller changes GS state or falls back to CPU work. */
 gx_cpu_wait(0);
#endif
 surface_counts[1]++;work_counts[0]++;work_counts[1]+=quads*passes;
 texture_counts[1]++;texture_counts[2]+=bytes;return 1;
}
static int surface_present(void *xfb,GXRModeObj *mode,uint32_t bp,uint32_t bw,uint32_t sx,uint32_t sy,uint32_t width,uint32_t height)
{
 if(!xfb||!mode||mode->aa||!surface||!surface->active||bp!=surface->bp||bw!=surface->bw||
    !width||!height||(width&3u)||(height&3u)||sx>surface->width||width>surface->width-sx||
    sy>surface->height||height>surface->height-sy||mode->fbWidth<surface->width||mode->efbHeight<surface->height)return 0;
 uint32_t copy_h=GX_SetDispCopyYScale(GX_GetYScaleFactor(mode->efbHeight,mode->xfbHeight));
 if(copy_h>mode->xfbHeight)return 0;
 if(!surface_snapshot())return 0;
 surface_state(surface->width,surface->height,1);surface_texture(surface_pixels,surface->width,surface->height,GX_NEAR);
 /* Keep resident RGB unmodified before the GX display-copy conversion. */
 surface_quad(surface->width,surface->height);
 source_cache.valid=0;surface_copy(texture,sx,sy,width,height);
 surface_state(mode->fbWidth,mode->efbHeight,1);surface_texture(texture,width,height,GX_LINEAR);
 GX_SetDispCopySrc(0,0,mode->fbWidth,mode->efbHeight);GX_SetDispCopyDst(mode->fbWidth,copy_h);
 GX_SetCopyFilter(GX_FALSE,mode->sample_pattern,GX_TRUE,mode->vfilter);
 GX_SetFieldMode(mode->field_rendering,mode->viHeight==2*mode->xfbHeight);GX_SetDispCopyGamma(GX_GM_1_0);
 surface_quad(mode->fbWidth,mode->efbHeight);texture_in_flight=1;
 GX_CopyDisp(xfb,GX_FALSE);gx_cpu_wait(2);
 surface_state(surface->width,surface->height,1);surface_texture(surface_pixels,surface->width,surface->height,GX_NEAR);
 surface_quad(surface->width,surface->height);surface_counts[4]++;return 1;
}

static int initialize(void)
{
    if(initialized)return 1;
    fifo=memalign(32,GX_PRESENT_FIFO);texture=memalign(32,GX_PRESENT_TEXTURE);
    if(!fifo||!texture){free(fifo);free(texture);fifo=texture=NULL;return 0;}
    memset(fifo,0,GX_PRESENT_FIFO);DCFlushRange(fifo,GX_PRESENT_FIFO);
    GX_Init(fifo,GX_PRESENT_FIFO);initialized=1;return 1;
}
static void vertex(float x,float y,float s,float t)
{GX_Position3f32(x,y,0);GX_TexCoord2f32(s,t);}
int gs_gx_present(void *xfb,GXRModeObj *mode,uint32_t bp,uint32_t bw,
                  uint32_t sx,uint32_t sy,uint32_t width,uint32_t height)
{
    /* Sparse targets resolve dirty tiles, then ordinary scanout reads coherent VRAM. */
    if(!gs_mem_sync())return 0;
    if(!xfb||!mode||!width||!height||width>1024u||height>512u||mode->aa||gx_wait_failed)return 0;
    if(!initialize())return 0;
    /* Previous copy finishes before CPU texture reuse; DrawDone below also
     * completes the EFB->XFB copy before the software FPS overlay writes. */
    if(texture_in_flight)gx_cpu_wait(0);
    source_cache.valid=0;
    uint32_t bytes=gs_gx_pack_rgba8(texture,GX_PRESENT_TEXTURE,bp,bw,sx,sy,width,height);
    if(!bytes)return 0;
    /* Scanout always repacks this buffer; it is not a source-cache hit. */
    DCFlushRange(texture,bytes);source_upload_pending=0;GX_InvalidateTexAll();
    GX_SetViewport(0,0,mode->fbWidth,mode->efbHeight,0,1);
    GX_SetScissor(0,0,mode->fbWidth,mode->efbHeight);
    GX_SetDispCopySrc(0,0,mode->fbWidth,mode->efbHeight);
    uint32_t copy_h=GX_SetDispCopyYScale(GX_GetYScaleFactor(mode->efbHeight,mode->xfbHeight));
    if(copy_h>mode->xfbHeight)return 0; /* Never exceed allocated XFB. */
    GX_SetDispCopyDst(mode->fbWidth,copy_h);
    GX_SetCopyFilter(GX_FALSE,mode->sample_pattern,GX_TRUE,mode->vfilter);
    GX_SetFieldMode(mode->field_rendering,mode->viHeight==2*mode->xfbHeight);
    GX_SetDispCopyGamma(GX_GM_1_0);GX_SetPixelFmt(GX_PF_RGB8_Z24,GX_ZC_LINEAR);
    GX_SetCullMode(GX_CULL_NONE);GX_SetZMode(GX_FALSE,GX_ALWAYS,GX_FALSE);GX_SetZTexture(GX_ZT_DISABLE,GX_TF_Z24X8,0);
    GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
    GX_SetColorUpdate(GX_TRUE);GX_SetAlphaUpdate(GX_FALSE);
    GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
    GX_SetNumChans(0);GX_SetNumTexGens(1);GX_SetNumTevStages(1);
    GX_SetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX2x4,GX_TG_TEX0,GX_IDENTITY);
    GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD0,GX_TEXMAP0,GX_COLORNULL);
    GX_SetTevOp(GX_TEVSTAGE0,GX_REPLACE);
    GX_ClearVtxDesc();GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);GX_SetVtxDesc(GX_VA_TEX0,GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_TEX0,GX_TEX_ST,GX_F32,0);
    Mtx model;Mtx44 projection;guMtxIdentity(model);
    guOrtho(projection,0,mode->efbHeight,0,mode->fbWidth,-1,1);
    GX_LoadPosMtxImm(model,GX_PNMTX0);GX_SetCurrentMtx(GX_PNMTX0);
    GX_LoadProjectionMtx(projection,GX_ORTHOGRAPHIC);
    GXTexObj object;uint32_t tw=(width+3u)&~3u,th=(height+3u)&~3u;
    GX_InitTexObj(&object,texture,tw,th,GX_TF_RGBA8,GX_CLAMP,GX_CLAMP,GX_FALSE);
    GX_InitTexObjLOD(&object,GX_LINEAR,GX_LINEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);
    GX_LoadTexObj(&object,GX_TEXMAP0);
    float s=(float)width/tw,t=(float)height/th;
    efb_width=mode->fbWidth;efb_height=mode->efbHeight;
    GXB(GX_QUADS,GX_VTXFMT0,4);
    vertex(0,0,0,0);vertex(mode->fbWidth,0,s,0);
    vertex(mode->fbWidth,mode->efbHeight,s,t);vertex(0,mode->efbHeight,0,t);
    GX_End();texture_in_flight=1;GX_CopyDisp(xfb,GX_FALSE);gx_cpu_wait(2);
    return 1;
}
static void color_vertex(float x,float y,uint32_t rgba)
{GX_Position3f32(x,y,0);GX_Color4u8(rgba,rgba>>8,rgba>>16,rgba>>24);}
int gs_gx_draw_flat_psm(uint32_t psm,uint32_t kind,uint32_t bp,uint32_t bw,int32_t minx,int32_t miny,
                    int32_t maxx,int32_t maxy,const int32_t *xy,uint32_t rgba,uint32_t scanmsk)
{
    if(psm>1u||!render_enabled||!initialized)return 0;
    work_counts[4]++;
    gs_gx_flat_draw prepared;
    if(!gs_gx_prepare_flat(&prepared,kind,bp,bw,minx,miny,maxx,maxy,xy,rgba,scanmsk)||
       prepared.width>efb_width||prepared.height>efb_height)return 0;
    prepared.psm=psm;
    if(surface_draw_flat(&prepared))return 1;
    if(!gs_mem_sync())return 0;
    flat_draw=prepared;
    /* Allocate/bind before drawing: failure must leave software fallback safe. */
    if(!readback)readback=memalign(32,GX_PRESENT_TEXTURE);
    if(!readback||!gs_mem_gpu_bind(resolve_capture,NULL))return 0;
    GX_SetViewport(0,0,flat_draw.width,flat_draw.height,0,1);
    GX_SetScissor(0,0,flat_draw.width,flat_draw.height);
    GX_SetPixelFmt(GX_PF_RGB8_Z24,GX_ZC_LINEAR);GX_SetDither(GX_FALSE);
    GX_SetFieldMode(GX_FALSE,GX_FALSE);GX_SetCullMode(GX_CULL_NONE);
    GX_SetZMode(GX_FALSE,GX_ALWAYS,GX_FALSE);GX_SetZTexture(GX_ZT_DISABLE,GX_TF_Z24X8,0);
    GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
    GX_SetColorUpdate(GX_TRUE);GX_SetAlphaUpdate(GX_FALSE);
    GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
    GX_SetNumChans(1);GX_SetNumTexGens(0);GX_SetNumTevStages(1);
    GX_SetChanCtrl(GX_COLOR0A0,GX_DISABLE,GX_SRC_REG,GX_SRC_VTX,GX_LIGHTNULL,GX_DF_NONE,GX_AF_NONE);
    GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0,GX_PASSCLR);
    GX_ClearVtxDesc();GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);GX_SetVtxDesc(GX_VA_CLR0,GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_CLR0,GX_CLR_RGBA,GX_RGBA8,0);
    Mtx model;Mtx44 projection;guMtxIdentity(model);
    guOrtho(projection,0,flat_draw.height,0,flat_draw.width,-1,1);
    GX_LoadPosMtxImm(model,GX_PNMTX0);GX_SetCurrentMtx(GX_PNMTX0);GX_LoadProjectionMtx(projection,GX_ORTHOGRAPHIC);
    /* Exact GS coverage is encoded as merged row-span quads. Avoid relying
     * on GX's triangle edge convention differing from the GS software oracle. */
    uint32_t quads=0;
    for(uint32_t y=0;y<flat_draw.height;) {
        uint32_t end=y+1u;
        while(end<flat_draw.height&&flat_draw.left[end]==flat_draw.left[y]&&
              flat_draw.right[end]==flat_draw.right[y])end++;
        if(flat_draw.right[y]>flat_draw.left[y])quads++;
        y=end;
    }
    /* At most 512 quads / 2048 vertices, safely within GX's u16 count.
     * Submit the exact same span rectangles in one FIFO primitive batch. */
    if(quads)GXB(GX_QUADS,GX_VTXFMT0,(uint16_t)(quads*4u));
    for(uint32_t y=0;y<flat_draw.height;) {
        uint32_t l=flat_draw.left[y],r=flat_draw.right[y],end=y+1u;
        while(end<flat_draw.height&&flat_draw.left[end]==l&&flat_draw.right[end]==r)end++;
        if(r>l){
            color_vertex(l,y,rgba);color_vertex(r,y,rgba);
            color_vertex(r,end,rgba);color_vertex(l,end,rgba);}
        y=end;
    }
    if(quads)GX_End();
    /* No uncovered EFB pixels are imported: previous VRAM and alpha remain. */
    if(!gs_gx_capture_vram_psmct32(bp,bw,minx,miny,flat_draw.width,flat_draw.height,rgba>>24))return 0;
    work_counts[0]++;work_counts[1]+=quads;work_counts[2]+=capture.bytes;
    capture_flat=1;return 1;
}
/* Snapshot depth/destination before issuing GPU work. Aliased color/depth
 * remains software, preserving write order. Z32 outside 24 bits stays hybrid. */
static int pipeline_prepare(gs_gx_texture_draw *d)
{
    gs_gx_pipeline *p=&d->pipeline;const uint8_t *vram=gs_mem_read_range(0,GS_MEM_SIZE);if(!vram)return 0;
    if(p->ztst>3u||p->a>2u||p->b>2u||p->c>2u||p->d>2u||p->fix>255u)return 0;
    uint32_t zlo=UINT32_MAX,zhi=0,flo=UINT32_MAX,fhi=0;
    unsigned destination_alpha=0,alpha_seen=0,alpha_constant=1;
    /* Z16/16S comparisons are exact in GX Z24. A fragment above 65535
     * can be represented by 65536, preserving every comparison result; the
     * authoritative GS write still uses the original fragment low 16 bits. */
    int exact24=(p->zpsm==2u||p->zpsm==10u)||
        (p->z<=0xffffffu&&(p->zpsm==0u||p->zpsm==1u));
    for(uint32_t y=0;y<d->rows;y++)for(uint32_t x=0;x<d->columns;x++) {
        uint32_t fo=(uint32_t)target_offset(d->coverage.bp,d->coverage.bw,d->coverage.x+x,d->coverage.y+y);
        if(fo<flo)flo=fo;if(fo+4u>fhi)fhi=fo+4u;
        if(p->blend&&p->c==1u&&d->coverage.psm==0u) {
            unsigned a=vram[fo+3];
            if(alpha_seen&&a!=destination_alpha)alpha_constant=0;
            destination_alpha=a;alpha_seen=1;
        }
        if(p->ztest||p->zwrite) {
            uint32_t zo=gs_mem_z_offset(p->zbp,d->coverage.bw,d->coverage.x+x,d->coverage.y+y,p->zpsm);
            if(zo==UINT32_MAX)return 0;
            if(zo<zlo)zlo=zo;if(zo+4u>zhi)zhi=zo+4u;
            if(raw_z(vram,zo,p->zpsm)>0xffffffu)exact24=0;
        }
    }
    if((p->ztest||p->zwrite)&&!(zhi<=flo||fhi<=zlo))return 0;
    /* DATE/ATE depends on the exact destination and shaded alpha of
     * each pixel. Do not let GX perform early Z writes or hardware blends:
     * the late software import handles AFAIL / DATE and the Z update. */
    if(p->date||p->ate){d->hardware_depth=0;d->hardware_blend=0;return 1;}
    d->hardware_depth=(p->ztest||p->zwrite)&&exact24;
    /* Z32 compares high 8 bits with late alpha and low 24 bits with GX Z.
     * Complementary high-less / high-equal passes preserve unsigned ordering.
     * Mixed PABE consumes late alpha itself, so that combination stays hybrid.
     * ALWAYS/NEVER need no high-bit comparison at all. */
    if(!exact24&&p->zpsm==0u&&(p->ztest||p->zwrite)&&!p->pabe)
        d->hardware_depth=p->ztest&&p->ztst>=2u?2u:1u;
    if(p->blend&&p->c==1u&&d->coverage.psm==0u&&alpha_seen&&alpha_constant) {
        p->c=2u;p->fix=destination_alpha;
    }
    /* R1299: every fixed coefficient is exact via seven integer midpoint
     * stages. Dynamic AS/AD still use the exact CPU resolver unless A=B. */
    int coeff=p->c==2u?(int)p->fix:p->c==1u&&d->coverage.psm==1u?128:-1;
    d->hardware_blend=p->blend&&p->colclamp&&(p->a==p->b||coeff>=0);
    if(d->hardware_blend) {
        if(!grow_texture(&destination_texture,&destination_capacity,d->coverage.width*d->coverage.height*4u))return 0;
        uint8_t *out=destination_texture;
        for(uint32_t y=0;y<d->coverage.height;y++)for(uint32_t x=0;x<d->coverage.width;x++) {
            uint32_t ix=x<d->columns?x:d->columns-1,iy=y<d->rows?y:d->rows-1;
            uint32_t off=(uint32_t)target_offset(d->coverage.bp,d->coverage.bw,d->coverage.x+ix,d->coverage.y+iy);
            uint32_t t=texture_tile(d->coverage.width,x,y);
            out[t]=vram[off+3];out[t+1]=vram[off];out[t+32]=vram[off+1];out[t+33]=vram[off+2];
        }
        DCFlushRange(destination_texture,d->coverage.width*d->coverage.height*4u);
    }
    if(d->hardware_depth) {
        if(!grow_texture(&depth_texture,&depth_capacity,d->coverage.width*d->coverage.height*4u))return 0;
        uint8_t *out=depth_texture;
        for(uint32_t y=0;y<d->coverage.height;y++)for(uint32_t x=0;x<d->coverage.width;x++) {
            uint32_t ix=x<d->columns?x:d->columns-1,iy=y<d->rows?y:d->rows-1;
            uint32_t z=raw_z(vram,gs_mem_z_offset(p->zbp,d->coverage.bw,d->coverage.x+ix,d->coverage.y+iy,p->zpsm),p->zpsm);
            uint32_t t=texture_tile(d->coverage.width,x,y);out[t]=d->hardware_depth==2u?z>>24:255;out[t+1]=z>>16;out[t+32]=z>>8;out[t+33]=z;
        }
        DCFlushRange(depth_texture,d->coverage.width*d->coverage.height*4u);
    }
    return 1;
}
/* Inspect the already GS-shaded snapshot, including exact source alpha.
 * Conservatively scan padding as well: extra variation can only reject an
 * optimization. Snapshot state changes are local; guest ALPHA/PABE stay intact. */
static void pipeline_source_alpha(gs_gx_texture_draw *d,const uint8_t *source)
{
    gs_gx_pipeline *p=&d->pipeline;
    if(!p->blend||(!p->pabe&&p->c!=0u))return;
    unsigned first=0,seen=0,constant=1,low=1,high=1;
    /* Only sampled passing fragments define AS/PABE. Triangle padding and
     * rejected Z fragments are not source pixels of this GS draw. */
    for(uint32_t y=0;y<d->rows;y++)for(uint32_t x=d->coverage.left[y];x<d->coverage.right[y];x++) {
        unsigned a=source[texture_tile(d->tw,d->column[x]-d->origin_x,d->row[y]-d->origin_y)];
        if(!seen){first=a;seen=1;}else if(a!=first)constant=0;if(a>=128u)low=0;else high=0;
    }
    if(!seen)return;
    if(p->pabe) {
        if(low) {p->pabe=0;p->a=p->b=p->d=0;p->c=2;p->fix=0;return;}
        if(high)p->pabe=0;
    }
    if(p->c==0u&&constant) {p->c=2u;p->fix=first;}
}
static void depth_preload(const gs_gx_texture_draw *d)
{
    GX_SetViewport(0,0,d->coverage.width,d->coverage.height,0,1);GX_SetScissor(0,0,d->coverage.width,d->coverage.height);
    GX_SetPixelFmt(GX_PF_RGB8_Z24,GX_ZC_LINEAR);GX_SetDither(GX_FALSE);GX_SetFieldMode(GX_FALSE,GX_FALSE);GX_SetCullMode(GX_CULL_NONE);
    GX_SetColorUpdate(GX_FALSE);GX_SetAlphaUpdate(GX_FALSE);GX_SetZMode(GX_TRUE,GX_ALWAYS,GX_TRUE);GX_SetZCompLoc(GX_FALSE);
    GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
    GX_SetNumChans(0);GX_SetNumTexGens(1);GX_SetNumTevStages(1);GX_SetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX2x4,GX_TG_TEX0,GX_IDENTITY);
    GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD0,GX_TEXMAP0,GX_COLORNULL);GX_SetTevOp(GX_TEVSTAGE0,GX_REPLACE);
    GX_SetZTexture(GX_ZT_REPLACE,GX_TF_Z24X8,0);GX_InvalidateTexAll();
    GX_ClearVtxDesc();GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);GX_SetVtxDesc(GX_VA_TEX0,GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_TEX0,GX_TEX_ST,GX_F32,0);
    Mtx model;Mtx44 projection;guMtxIdentity(model);guOrtho(projection,0,d->coverage.height,0,d->coverage.width,-1,1);
    GX_LoadPosMtxImm(model,GX_PNMTX0);GX_SetCurrentMtx(GX_PNMTX0);GX_LoadProjectionMtx(projection,GX_ORTHOGRAPHIC);
    GXTexObj obj;GX_InitTexObj(&obj,depth_texture,d->coverage.width,d->coverage.height,GX_TF_RGBA8,GX_CLAMP,GX_CLAMP,GX_FALSE);
    GX_InitTexObjLOD(&obj,GX_NEAR,GX_NEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);GX_LoadTexObj(&obj,GX_TEXMAP0);
    GXB(GX_QUADS,GX_VTXFMT0,4);vertex(0,0,0,0);vertex(d->coverage.width,0,1,0);
    vertex(d->coverage.width,d->coverage.height,1,1);vertex(0,d->coverage.height,0,1);GX_End();
    GX_SetZTexture(GX_ZT_DISABLE,GX_TF_Z24X8,0);
}
static u8 blend_input(uint32_t v){return v==0u?GX_CC_C0:v==1u?GX_CC_C1:GX_CC_ZERO;}
static void tev_copy(u8 stage,u8 map,u8 coord,u8 reg)
{
    GX_SetTevOrder(stage,coord,map,GX_COLORNULL);
    GX_SetTevColorIn(stage,GX_CC_ZERO,GX_CC_ZERO,GX_CC_ZERO,GX_CC_TEXC);
    GX_SetTevColorOp(stage,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_FALSE,reg);
    GX_SetTevAlphaIn(stage,GX_CA_ZERO,GX_CA_ZERO,GX_CA_ZERO,GX_CA_TEXA);
    GX_SetTevAlphaOp(stage,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_FALSE,reg);
}
static void pipeline_shader_target(const gs_gx_texture_draw *d,void *destination,uint32_t destination_w,uint32_t destination_h)
{
    unsigned stages=1;const gs_gx_pipeline *p=&d->pipeline;
    if(d->hardware_blend) {
        GX_SetNumTexGens(2);GX_SetTexCoordGen(GX_TEXCOORD1,GX_TG_MTX2x4,GX_TG_TEX1,GX_IDENTITY);
        GX_SetVtxDesc(GX_VA_TEX1,GX_DIRECT);GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_TEX1,GX_TEX_ST,GX_F32,0);
        GXTexObj obj;GX_InitTexObj(&obj,destination,destination_w,destination_h,GX_TF_RGBA8,GX_CLAMP,GX_CLAMP,GX_FALSE);
        GX_InitTexObjLOD(&obj,GX_NEAR,GX_NEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);GX_LoadTexObj(&obj,GX_TEXMAP1);
        tev_copy(0,GX_TEXMAP0,GX_TEXCOORD0,GX_TEVREG0);tev_copy(1,GX_TEXMAP1,GX_TEXCOORD1,GX_TEVREG1);
        unsigned coeff=p->c==2u?p->fix:128u;
        u8 A=blend_input(p->a),B=blend_input(p->b),D=blend_input(p->d);
        if(p->a==p->b||coeff==0u||coeff==128u) {
            /* Preserve the short endpoint path. */
            if(coeff==0u||p->a==p->b)A=B=GX_CC_ZERO;
            GX_SetTevOrder(2,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLORNULL);
            GX_SetTevColorIn(2,A,A,GX_CC_ZERO,D);GX_SetTevColorOp(2,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_FALSE,GX_TEVREG2);
            GX_SetTevAlphaIn(2,GX_CA_ZERO,GX_CA_ZERO,GX_CA_ZERO,GX_CA_A0);GX_SetTevAlphaOp(2,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_FALSE,GX_TEVREG2);
            GX_SetTevOrder(3,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLORNULL);
            GX_SetTevColorIn(3,B,B,GX_CC_ZERO,GX_CC_C2);GX_SetTevColorOp(3,GX_TEV_SUB,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
            GX_SetTevAlphaIn(3,GX_CA_ZERO,GX_CA_ZERO,GX_CA_ZERO,GX_CA_A0);GX_SetTevAlphaOp(3,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
            stages=4;
        } else {
            /* Let r=C mod 128. Seven exact integer midpoints, LSB first,
             * produce H=floor(((128-r)*B+r*A)/128). Each stage computes
             * (H+chosen)>>1 with A==B in the TEV lerp, so neither 8-bit
             * coefficient expansion nor fractional rounding can affect it.
             * Then H-B+D (+A-B for C>=128) is the GS signed >>7 equation.
             * Midpoints stay 0..255. Signed sums use TEV's D input (11 bits),
             * never its masked A/B/C inputs. Maximum is 13 stages, 14 with Z. */
            stages=2;
            unsigned r=coeff&127u;
            for(unsigned bit=0;bit<7;bit++) {
                u8 chosen=(r&(1u<<bit))?A:B;
                GX_SetTevOrder(stages,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLORNULL);
                GX_SetTevColorIn(stages,chosen,chosen,GX_CC_ZERO,bit?GX_CC_C2:B);
                GX_SetTevColorOp(stages,GX_TEV_ADD,GX_TB_ZERO,GX_CS_DIVIDE_2,GX_FALSE,GX_TEVREG2);
                GX_SetTevAlphaIn(stages,GX_CA_ZERO,GX_CA_ZERO,GX_CA_ZERO,GX_CA_A0);
                GX_SetTevAlphaOp(stages,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_FALSE,GX_TEVREG2);
                stages++;
            }
            /* D-B, then the optional extra A-B, then add H and clamp once. */
            unsigned tail=coeff>=128u?4u:2u;
            for(unsigned k=0;k<tail;k++) {
                u8 input=k==0u?B:k==tail-1u?GX_CC_C2:k==1u?A:B;
                u8 op=k==0u||(k==2u&&tail==4u)?GX_TEV_SUB:GX_TEV_ADD;
                GX_SetTevOrder(stages,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLORNULL);
                GX_SetTevColorIn(stages,input,input,GX_CC_ZERO,k?GX_CC_CPREV:D);
                GX_SetTevColorOp(stages,op,GX_TB_ZERO,GX_CS_SCALE_1,k==tail-1u?GX_TRUE:GX_FALSE,GX_TEVPREV);
                GX_SetTevAlphaIn(stages,GX_CA_ZERO,GX_CA_ZERO,GX_CA_ZERO,GX_CA_A0);
                GX_SetTevAlphaOp(stages,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
                stages++;
            }
        }
    }
    if(d->hardware_depth) {
        if(d->hardware_depth==2u) {
            /* Keep the exact old high byte for the late alpha predicate.
             * The following zero texture still supplies constant fragment Z. */
            GX_SetNumTexGens(2);GX_SetTexCoordGen(GX_TEXCOORD1,GX_TG_MTX2x4,GX_TG_TEX1,GX_IDENTITY);
            GX_SetVtxDesc(GX_VA_TEX1,GX_DIRECT);GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_TEX1,GX_TEX_ST,GX_F32,0);
            GXTexObj high;GX_InitTexObj(&high,depth_texture,d->coverage.width,d->coverage.height,GX_TF_RGBA8,GX_CLAMP,GX_CLAMP,GX_FALSE);
            GX_InitTexObjLOD(&high,GX_NEAR,GX_NEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);GX_LoadTexObj(&high,GX_TEXMAP3);
            GX_SetTevOrder(stages,GX_TEXCOORD1,GX_TEXMAP3,GX_COLORNULL);
            GX_SetTevColorIn(stages,GX_CC_ZERO,GX_CC_ZERO,GX_CC_ZERO,GX_CC_CPREV);
            GX_SetTevColorOp(stages,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
            GX_SetTevAlphaIn(stages,GX_CA_ZERO,GX_CA_ZERO,GX_CA_ZERO,GX_CA_TEXA);
            GX_SetTevAlphaOp(stages,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
            stages++;
        }
        memset(zzero,0,sizeof(zzero));DCFlushRange(zzero,sizeof(zzero));
        GXTexObj obj;GX_InitTexObj(&obj,zzero,4,4,GX_TF_RGBA8,GX_CLAMP,GX_CLAMP,GX_FALSE);
        GX_InitTexObjLOD(&obj,GX_NEAR,GX_NEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);GX_LoadTexObj(&obj,GX_TEXMAP2);
        GX_SetTevOrder(stages,GX_TEXCOORD0,GX_TEXMAP2,GX_COLORNULL);GX_SetTevOp(stages,GX_PASSCLR);
        GX_SetTevColorIn(stages,GX_CC_ZERO,GX_CC_ZERO,GX_CC_ZERO,GX_CC_CPREV);
        GX_SetTevAlphaIn(stages,GX_CA_ZERO,GX_CA_ZERO,GX_CA_ZERO,GX_CA_APREV);
        uint32_t fragment_z=(p->zpsm==2u||p->zpsm==10u)&&p->z>65535u?65536u:p->z&0xffffffu;
        GX_SetZCompLoc(GX_FALSE);GX_SetZTexture(GX_ZT_REPLACE,GX_TF_Z24X8,fragment_z);
        GX_SetZMode(GX_TRUE,!p->ztest||p->ztst==1u?GX_ALWAYS:p->ztst==0u?GX_NEVER:p->ztst==2u?GX_GEQUAL:GX_GREATER,p->zwrite?GX_TRUE:GX_FALSE);
        stages++;
    }
    GX_SetNumTevStages(stages);
}
static void pipeline_shader(const gs_gx_texture_draw *d)
{pipeline_shader_target(d,destination_texture,d->coverage.width,d->coverage.height);}

/* Keep color on GX, and exact GS Z in a disjoint CPU shadow. Unsupported
 * fragmented masks/alpha coefficients return to the existing compact path. */
static int surface_draw_pipeline(gs_gx_texture_draw *d,gs_gx_texel_fn sample,const gs_gx_pipeline *input)
{
 if(input&&(input->ate||input->date))return 0; /* never skip late TEST/DATE in resident EFB */
 if(!input||input->fbmask||input->fba)return 0; /* unsupported late writes remain on the software GS path */
 resident_pipeline_counts[0]++;
 if(!sample||!input)return 0;
#ifdef GEKKO2_GX_RESIDENT_BLEND_DISABLE
 if(input->blend){resident_pipeline_counts[8]++;return 0;}
#endif
#ifdef GEKKO2_GX_NATIVE_ADDITIVE
 if(input->blend&&!gs_gx_additive_supported(input)){resident_pipeline_counts[8]++;return 0;}
#endif
#ifdef GEKKO2_GX_DUMP
 gs_gx_dump_record("RESIDENT_TARGET",d->coverage.bp,d->coverage.bw,d->coverage.x,d->coverage.y);
 gs_gx_dump_record("RESIDENT_STATE",d->coverage.width,d->coverage.height,input->blend,input->z);
#endif
 d->pipeline=*input;
 uint32_t h=efb_height<512u?efb_height&~3u:512u;
 if(!h||d->coverage.bw>efb_width||d->coverage.bw>640u||
    d->coverage.x+d->coverage.width>d->coverage.bw||d->coverage.y+d->coverage.height>h||
    !gs_gx_target_valid(GS_MEM_SIZE,d->coverage.bp,d->coverage.bw,0,0,d->coverage.bw,h)) {
  resident_pipeline_counts[9]++;return 0;
 }
 uint32_t clo=d->coverage.bp*4u,chi=(uint32_t)target_offset(d->coverage.bp,d->coverage.bw,d->coverage.bw-1,h-1)+4u;
 uint32_t zlo=0,zhi=0;uint64_t tested=0,failed=0;
 if((input->ztest||input->zwrite)&&!gs_gx_surface_depth_range(d,input,clo,chi,&zlo,&zhi)) {
  resident_pipeline_counts[7]++;return 0;
 }
 /* Read the depth range before opening/changing the resident target. Any
  * overlap with the previous target follows the normal resolve barrier. */
 const uint8_t *depth=NULL;
 if(input->ztest&&input->ztst>=2u&&zhi) {
  depth=gs_mem_read_range(zlo,zhi-zlo);if(!depth)return 0;
 }
 if(!gs_gx_surface_depth_clip(d,input,depth,zlo,zhi-zlo,&tested,&failed)) {
  resident_pipeline_counts[7]++;return 0;
 }
 gs_gx_pipeline *p=&d->pipeline;
 if(p->blend&&(!p->colclamp||p->a>2u||p->b>2u||p->c>2u||p->d>2u||p->fix>255u)) {
  resident_pipeline_counts[8]++;return 0;
 }
 if((!surface||!surface->active)&&!gs_mem_sync())return 0;
 int additive=0;
#ifdef GEKKO2_GX_NATIVE_ADDITIVE
 additive=gs_gx_additive_supported(input);
#endif
 if(additive){
  /* Never reuse an unscaled snapshot or scale a cached snapshot twice. */
  source_key_valid=source_cache.valid=0;
  additive_source=sample;additive_fix=input->fix;sample=additive_sample;
 }
 uint32_t bytes=source_pack(d,sample);if(!bytes)return 0;
 pipeline_source_alpha(d,texture);
 if(!surface_ensure(&d->coverage)){resident_pipeline_counts[9]++;return 0;}
 if(p->blend&&p->c==1u&&d->coverage.psm==0u&&p->a!=p->b) {
  unsigned seen=0,value=0;
  for(uint32_t y=0;y<d->rows;y++)for(uint32_t x=d->coverage.left[y];x<d->coverage.right[y];x++) {
   unsigned a=surface->alpha[(d->coverage.y+y)*surface->width+d->coverage.x+x];
   if(seen&&a!=value){resident_pipeline_counts[8]++;return 0;}value=a;seen=1;
  }
  p->c=2u;p->fix=value;
 }
 if(p->blend&&p->c==0u&&p->a!=p->b){resident_pipeline_counts[8]++;return 0;}
 d->hardware_blend=additive?2u:p->blend;d->hardware_depth=0;
 /* CPU preplanned every passing fragment. GX does RGB/TEV, not the Z test. */
 p->ztest=0;p->zwrite=0;
 if(!surface_draw_textured(d,bytes))return 0;
 if(additive){additive_counts[0]++;additive_counts[1]+=(uint64_t)d->columns*d->rows;additive_counts[2]+=(uint64_t)d->coverage.width*d->coverage.height*4u;}
 uint64_t written=0;
 if(input->zwrite)for(uint32_t y=0;y<d->rows;y++)for(uint32_t x=d->coverage.left[y];x<d->coverage.right[y];x++) {
  gs_mem_write_z(input->zbp,d->coverage.bw,d->coverage.x+x,d->coverage.y+y,input->zpsm,gs_mem_clamp_raster_z(input->z,input->zpsm));written++;
 }
 resident_pipeline_counts[1]++;resident_pipeline_counts[2]+=tested;resident_pipeline_counts[3]+=failed;
 resident_pipeline_counts[4]+=written;resident_pipeline_counts[5]+=!!input->blend;
 resident_pipeline_counts[6]+=(uint64_t)d->coverage.width*d->coverage.height*4u;
 if(input->ztest||input->zwrite)pipeline_counts[1]++;
 if(input->blend)pipeline_counts[2]++;
 return 1;
}
static uint64_t native_affine_stats[11];
uint64_t gs_gx_native_affine_stat(unsigned n){return n<11u?native_affine_stats[n]:0;}
static uint8_t native_alpha_texels[128u*128u];
static uint32_t native_alpha_function(uint32_t a,const gs_gx_native_color *c)
{
 if(!c)return a;
 uint32_t va=c->rgba>>24;
 if(!c->tcc)return va;
 if(c->tfx==0u)a=a*va/128u;
 else if(c->tfx==2u)a+=va;
 return a>255u?255u:a;
}
static int32_t native_wrap(int32_t n,uint32_t size,uint32_t clamp)
{return clamp?(n<0?0:(n>=(int32_t)size?(int32_t)size-1:n)):(n&(size-1u));}
static uint32_t native_alpha_at(double u,double v,uint32_t tw,uint32_t th,uint32_t ws,uint32_t wt,uint32_t linear)
{
 if(linear){u-=0.5;v-=0.5;}
 if(!(u>-2147483647.0&&u<2147483646.0&&v>-2147483647.0&&v<2147483646.0))return 0;
 int32_t x=(int32_t)u,y=(int32_t)v;if(u<x)x--;if(v<y)y--;
 uint32_t x0=native_wrap(x,tw,ws),y0=native_wrap(y,th,wt);
 uint32_t a=native_alpha_texels[y0*tw+x0];if(!linear)return a;
 uint32_t fx=(uint32_t)((u-x)*16.0),fy=(uint32_t)((v-y)*16.0);
 uint32_t x1=native_wrap(x+1,tw,ws),y1=native_wrap(y+1,th,wt);
 uint32_t top=(a*(16u-fx)+native_alpha_texels[y0*tw+x1]*fx)>>4;
 uint32_t bot=(native_alpha_texels[y1*tw+x0]*(16u-fx)+native_alpha_texels[y1*tw+x1]*fx)>>4;
 return (top*(16u-fy)+bot*fy)>>4;
}
static int32_t native_edge(int32_t ax,int32_t ay,int32_t bx,int32_t by,int32_t x,int32_t y)
{return (x-ax)*(by-ay)-(y-ay)*(bx-ax);}
static uint32_t native_fragment_alpha(int32_t x,int32_t y,const int32_t *xy,const float *uv,const float *q,
 uint32_t tw,uint32_t th,uint32_t ws,uint32_t wt,uint32_t linear,const gs_gx_native_color *c,double inv)
{
 double b0=(double)native_edge(xy[2],xy[3],xy[4],xy[5],x,y)*inv;
 double b1=(double)native_edge(xy[4],xy[5],xy[0],xy[1],x,y)*inv;
 double b2=(double)native_edge(xy[0],xy[1],xy[2],xy[3],x,y)*inv;
 double s=b0*uv[0]+b1*uv[2]+b2*uv[4],t=b0*uv[1]+b1*uv[3]+b2*uv[5];
 if(q){double qi=b0*q[0]+b1*q[1]+b2*q[2];double iq=qi!=0?1.0/qi:0;s*=iq;t*=iq;}
 return native_alpha_function(native_alpha_at(s*tw,t*th,tw,th,ws,wt,linear),c);
}
static int native_triangle(uint32_t psm,uint32_t bp,uint32_t bw,
 int32_t minx,int32_t miny,int32_t maxx,int32_t maxy,const int32_t *xy,
 const float *uv,const float *q,const uint32_t *rgba,uint32_t tw,uint32_t th,uint32_t ws,uint32_t wt,
 uint32_t linear,uint32_t scanmsk,gs_gx_texel_fn sample,const gs_gx_pipeline *p,uint32_t conservative_z,const gs_gx_native_color *color)
{
#if !defined(GEKKO2_GX_NATIVE_BIOS_TRI)
 (void)color;(void)conservative_z;(void)psm;(void)bp;(void)bw;(void)minx;(void)miny;(void)maxx;(void)maxy;(void)xy;(void)uv;
 (void)q;(void)rgba;(void)tw;(void)th;(void)ws;(void)wt;(void)linear;(void)scanmsk;(void)sample;(void)p;return 0;
#else
 native_affine_stats[0]++;
 uint32_t constant_colors[3];int vertex_gouraud=rgba!=NULL;
 if(color&&color->tfx>1u)return 0; /* HIGHLIGHT needs another TEV stage. */
 if(color&&color->tfx==0u&&!rgba){
  constant_colors[0]=constant_colors[1]=constant_colors[2]=color->rgba;
  rgba=constant_colors; /* MODULATE runs after filtering in GX, including flat color. */
 }

 if(!gs_gx_render_active()||!surface_enabled||!xy||!uv||!sample||!p||psm>1u||
    tw<4u||th<4u||tw>128u||th>128u||(tw&(tw-1u))||(th&(th-1u))||ws>1u||wt>1u||
    (conservative_z&&p->zwrite)||p->pabe||p->ztst>3u||!p->colclamp||p->a>2u||p->b>2u||p->c>2u||p->d>2u||p->fix>255u)
    return 0;
 for(unsigned i=0;i<6;i++)if(!isfinite(uv[i])||fabsf(uv[i])>65536.0f)return 0;
 Mtx tm;int projective=q!=NULL;
 if(rgba&&((rgba[0]>>24)!=(rgba[1]>>24)||(rgba[1]>>24)!=(rgba[2]>>24)))return 0;
 if(projective){
  double dx1=(double)xy[2]-xy[0],dy1=(double)xy[3]-xy[1];
  double dx2=(double)xy[4]-xy[0],dy2=(double)xy[5]-xy[1],det=dx1*dy2-dx2*dy1;
  if(!isfinite(det)||fabs(det)<1.0)return 0;
  for(unsigned v=0;v<3;v++)if(!isfinite(q[v])||q[v]<=0.0f)return 0;
  for(unsigned row=0;row<3;row++){
   double f0=row==2?q[0]:uv[row],f1=row==2?q[1]:uv[2+row],f2=row==2?q[2]:uv[4+row];
   double a=((f1-f0)*dy2-(f2-f0)*dy1)/det,b=(dx1*(f2-f0)-dx2*(f1-f0))/det;
   double c=f0-a*(xy[0]+0.5)-b*(xy[1]+0.5);
   if(!isfinite(a)||!isfinite(b)||!isfinite(c)||fabs(a)>65536||fabs(b)>65536||fabs(c)>65536)return 0;
   tm[row][0]=(float)a;tm[row][1]=(float)b;tm[row][2]=0;tm[row][3]=(float)c;
  }
 }
 int additive=p->blend&&gs_gx_additive_supported(p);
 int additive_alpha=p->blend&&p->a==0u&&p->b==2u&&p->c==0u&&p->d==1u;
 int alpha_blend=p->blend&&p->a==0u&&p->b==1u&&p->d==1u&&
                 (p->c==0u||(p->c==2u&&p->fix<=128u));
 if(p->blend&&!additive&&!alpha_blend&&!additive_alpha)return 0;
 gs_gx_texture_draw *d=&texture_draw;memset(d,0,sizeof(*d));
 if(!gs_gx_prepare_flat(&d->coverage,3,bp,bw,minx,miny,maxx,maxy,xy,0,scanmsk))return 0;
 d->coverage.psm=psm;d->tw=tw;d->th=th;d->columns=d->coverage.width;d->rows=d->coverage.height;
 if(tw*th>d->columns*d->rows*4u)return 0;
 uint32_t h=efb_height<512u?efb_height&~3u:512u;
 if(color&&color->target_height){
  uint32_t gh=(color->target_height+3u)&~3u;
  if(color->target_height>512u)return 0;
  if(gh<h)h=gh;
 }
 if(!h||bw>efb_width||bw>640u||d->coverage.x+d->coverage.width>bw||d->coverage.y+d->coverage.height>h||
    !gs_gx_target_valid(GS_MEM_SIZE,bp,bw,0,0,bw,h))return 0;
 d->pipeline=*p;uint32_t clo=bp*4u,chi=(uint32_t)target_offset(bp,bw,bw-1u,h-1u)+4u;
 uint32_t zlo=0,zhi=0;uint64_t tested=0,failed=0;const uint8_t *depth=NULL;
 if((p->ztest||p->zwrite)&&!gs_gx_surface_depth_range(d,p,clo,chi,&zlo,&zhi))return 0;
 if(p->ztest&&p->ztst>=2u&&zhi){depth=gs_mem_read_range(zlo,zhi-zlo);if(!depth)return 0;}
 if(!gs_gx_surface_depth_clip(d,p,depth,zlo,zhi-zlo,&tested,&failed))return 0;
 /* Variable Z: the caller supplies a lower bound. Every covered sample must
  * pass that bound, and Z writes are forbidden; otherwise scalar fallback. */
 if(conservative_z&&failed)return 0;
 /* Pack raw, constant-color GS texels once, never shaded screen pixels. */
 source_key_valid=source_cache.valid=0;
 if(texture_in_flight)gx_cpu_wait(0);
 uint32_t alpha=0,bytes=tw*th*4u;
 int uniform_required=!color&&(!psm||(alpha_blend&&p->c==0u));
 int varying_alpha=0;uint32_t max_alpha=0,min_alpha=255;
 /* R1389: stop at the first differing alpha; do not decode the rest
  * of a texture that is guaranteed to fall back. No target changes yet. */
 for(uint32_t y=0;y<th;y++)for(uint32_t x=0;x<tw;x++){
  uint32_t c=sample((int32_t)x,(int32_t)y),a=c>>24,k=texture_tile(tw,x,y);
  native_affine_stats[3]++;
  if(!x&&!y)alpha=a;
  else if(a!=alpha){if(uniform_required){native_affine_stats[4]++;return 0;}varying_alpha=1;}
  native_alpha_texels[y*tw+x]=a;
  uint32_t fa=native_alpha_function(a,color);if(fa>max_alpha)max_alpha=fa;if(fa<min_alpha)min_alpha=fa;
  if(color&&alpha_blend&&p->c==0u&&fa>128u){native_affine_stats[4]++;return 0;}
  if(color&&!rgba){
   uint32_t out=0,va=color->rgba>>24;
   for(unsigned k=0;k<3;k++){
    uint32_t ch=(c>>(k*8))&255u;
    if(color->tfx!=1u){ch=ch*((color->rgba>>(k*8))&255u)/128u;if(color->tfx>=2u)ch+=va;}
    if(ch>255u)ch=255u;out|=ch<<(k*8);
   }c=(a<<24)|out;
  }
  uint8_t *v=texture;v[k]=a;v[k+1]=c;v[k+32]=c>>8;v[k+33]=c>>16;
 }
 source_upload_pending=1u;
 if(color){alpha=native_alpha_function(alpha,color);varying_alpha=min_alpha!=max_alpha;}
 else if(rgba)alpha=(alpha*(rgba[0]>>24))>>7;
 if(alpha>255u)alpha=255u;
 if(alpha_blend){
  uint32_t factor=p->c==2u?p->fix:alpha;if((p->c==0u?max_alpha:factor)>128u)return 0;
  /* GS alpha uses 128 for unity; GX uses 255. This test build maps
   * the constant coefficient, retaining original GS alpha metadata. */
  uint8_t gx_alpha=(uint8_t)((factor*255u+64u)/128u);
  for(uint32_t y=0;y<th;y++)for(uint32_t x=0;x<tw;x++){
   uint32_t a=(color&&p->c==0u)?(native_alpha_function(native_alpha_texels[y*tw+x],color)*255u+64u)/128u:gx_alpha;
   ((uint8_t*)texture)[texture_tile(tw,x,y)]=(uint8_t)a;
  }
 }
 if(additive_alpha&&color)for(uint32_t y=0;y<th;y++)for(uint32_t x=0;x<tw;x++)
  ((uint8_t*)texture)[texture_tile(tw,x,y)]=(uint8_t)native_alpha_function(native_alpha_texels[y*tw+x],color);
 if(additive)for(uint32_t y=0;y<th;y++)for(uint32_t x=0;x<tw;x++) {
   uint32_t k=texture_tile(tw,x,y);uint8_t *v=texture;
   v[k+1]=((uint32_t)v[k+1]*p->fix)>>7;
   v[k+32]=((uint32_t)v[k+32]*p->fix)>>7;
   v[k+33]=((uint32_t)v[k+33]*p->fix)>>7;
 }
 if(!surface_ensure_height(&d->coverage,h))return 0;
 d->coverage.rgba=alpha<<24;
 if(!gs_gx_surface_mark(surface,&d->coverage))return 0;
 if(color&&varying_alpha&&!psm){
  double inv=1.0/(double)native_edge(xy[0],xy[1],xy[2],xy[3],xy[4],xy[5]);
  for(uint32_t y=0;y<d->rows;y++)for(uint32_t x=d->coverage.left[y];x<d->coverage.right[y];x++){
   uint32_t px=d->coverage.x+x,py=d->coverage.y+y;
   surface->alpha[py*surface->bw+px]=(uint8_t)native_fragment_alpha(px,py,xy,uv,q,tw,th,ws,wt,linear,color,inv);
   native_affine_stats[9]++;
  }
 }

 source_upload(bytes);surface_state(surface->width,surface->height,1);
 GXTexObj obj;GX_InitTexObj(&obj,texture,tw,th,GX_TF_RGBA8,ws?GX_CLAMP:GX_REPEAT,wt?GX_CLAMP:GX_REPEAT,GX_FALSE);
 GX_InitTexObjLOD(&obj,linear?GX_LINEAR:GX_NEAR,linear?GX_LINEAR:GX_NEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);
 GX_LoadTexObj(&obj,GX_TEXMAP0);
 if(projective){GX_LoadTexMtxImm(tm,GX_TEXMTX0,GX_MTX3x4);
  GX_SetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX3x4,GX_TG_POS,GX_TEXMTX0);}
 if(rgba){
  GX_SetNumChans(1);GX_SetChanCtrl(GX_COLOR0A0,GX_DISABLE,GX_SRC_REG,GX_SRC_VTX,GX_LIGHTNULL,GX_DF_NONE,GX_AF_NONE);
  GX_SetVtxDesc(GX_VA_CLR0,GX_DIRECT);GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_CLR0,GX_CLR_RGBA,GX_RGBA8,0);
  GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD0,GX_TEXMAP0,GX_COLOR0A0);
  GX_SetTevColorIn(GX_TEVSTAGE0,GX_CC_ZERO,GX_CC_TEXC,GX_CC_RASC,GX_CC_ZERO);
  GX_SetTevColorOp(GX_TEVSTAGE0,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_2,GX_TRUE,GX_TEVPREV);
 }

 if(additive_alpha){
  /* The BIOS fog uses (Cs - 0)*As + Cd. Keep the filtered RGB and
   * filtered alpha separate until TEV multiplication, then ONE+ONE.
   * Pre-multiplying source texels would alter bilinear edge colors. */
  GX_SetNumTevStages(2);
  GX_SetTevOrder(GX_TEVSTAGE1,GX_TEXCOORD0,GX_TEXMAP0,GX_COLORNULL);
  GX_SetTevColorIn(GX_TEVSTAGE1,GX_CC_ZERO,GX_CC_CPREV,GX_CC_TEXA,GX_CC_ZERO);
  GX_SetTevColorOp(GX_TEVSTAGE1,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_2,GX_TRUE,GX_TEVPREV);
  GX_SetTevAlphaIn(GX_TEVSTAGE1,GX_CA_ZERO,GX_CA_ZERO,GX_CA_ZERO,GX_CA_APREV);
  GX_SetTevAlphaOp(GX_TEVSTAGE1,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
  GX_SetBlendMode(GX_BM_BLEND,GX_BL_ONE,GX_BL_ONE,GX_LO_COPY);
 }
 if(additive)GX_SetBlendMode(GX_BM_BLEND,GX_BL_ONE,GX_BL_ONE,GX_LO_COPY);
 if(alpha_blend)GX_SetBlendMode(GX_BM_BLEND,GX_BL_SRCALPHA,GX_BL_INVSRCALPHA,GX_LO_COPY);
 /* Exact CPU coverage/depth mask, but hardware texture sampling and RGB.
  * Group matching rows; no per-output-pixel texture or color callback. */
 uint32_t submitted=0;
 for(uint32_t y=0;y<d->rows;){uint32_t end=y+1,l=d->coverage.left[y],r=d->coverage.right[y];
   while(end<d->rows&&d->coverage.left[end]==l&&d->coverage.right[end]==r)end++;
   if(r>l){GX_SetScissor(d->coverage.x+l,d->coverage.y+y,r-l,end-y);
    GXB(GX_TRIANGLES,GX_VTXFMT0,3);
    for(unsigned v=0;v<3;v++){
     GX_Position3f32((float)xy[v*2]+0.5f,(float)xy[v*2+1]+0.5f,0);
     if(rgba)GX_Color4u8(rgba[v]&255u,(rgba[v]>>8)&255u,(rgba[v]>>16)&255u,rgba[v]>>24);
     GX_TexCoord2f32(uv[v*2],uv[v*2+1]);
    }
    GX_End();submitted+=3u;}y=end;
 }
 GX_SetScissor(0,0,surface->width,surface->height);texture_in_flight=1;surface_texture_valid=0;
 if(!gs_mem_gpu_mark_pending())return 0;
 if(p->zwrite)for(uint32_t y=0;y<d->rows;y++)for(uint32_t x=d->coverage.left[y];x<d->coverage.right[y];x++)
   gs_mem_write_z(p->zbp,d->coverage.bw,d->coverage.x+x,d->coverage.y+y,p->zpsm,gs_mem_clamp_raster_z(p->z,p->zpsm));
 native_affine_stats[1]++;native_affine_stats[2]+=(uint64_t)d->columns*d->rows;
 native_affine_stats[5]+=submitted;
 native_affine_stats[6]+=projective;native_affine_stats[7]+=vertex_gouraud;native_affine_stats[8]+=color&&varying_alpha;native_affine_stats[10]+=additive_alpha;
 resident_pipeline_counts[2]+=tested;resident_pipeline_counts[3]+=failed;
 return 1;
#endif
}

int gs_gx_draw_native_affine_triangle(uint32_t psm,uint32_t bp,uint32_t bw,
 int32_t minx,int32_t miny,int32_t maxx,int32_t maxy,const int32_t *xy,
 const float *uv,uint32_t tw,uint32_t th,uint32_t ws,uint32_t wt,
 uint32_t linear,uint32_t scanmsk,gs_gx_texel_fn sample,const gs_gx_pipeline *p)
{return native_triangle(psm,bp,bw,minx,miny,maxx,maxy,xy,uv,NULL,NULL,tw,th,ws,wt,linear,scanmsk,sample,p,0,NULL);}
int gs_gx_draw_native_stq_triangle(uint32_t psm,uint32_t bp,uint32_t bw,
 int32_t minx,int32_t miny,int32_t maxx,int32_t maxy,const int32_t *xy,
 const float *uv,const float *q,const uint32_t *rgba,uint32_t tw,uint32_t th,
 uint32_t ws,uint32_t wt,uint32_t linear,uint32_t scanmsk,
 gs_gx_texel_fn sample,const gs_gx_pipeline *p,uint32_t conservative_z,const gs_gx_native_color *color)
{return native_triangle(psm,bp,bw,minx,miny,maxx,maxy,xy,uv,q,rgba,tw,th,ws,wt,linear,scanmsk,sample,p,conservative_z,color);}

static void pipeline_vertex(float x,float y,float s,float t,const gs_gx_texture_draw *d)
{
    vertex(x,y,s,t);if(d->hardware_blend||d->hardware_depth==2u)GX_TexCoord2f32(x/d->coverage.width,y/d->coverage.height);
}
int gs_gx_draw_texture_sprite(uint32_t psm,uint32_t bp,uint32_t bw,
    int32_t x,int32_t y,uint32_t w,uint32_t h,const int32_t *columns,const int32_t *rows,
    double step_x,double step_y,uint32_t scanmsk,gs_gx_texel_fn sample,const gs_gx_pipeline *pipeline)
{
    if(!render_enabled||!initialized)return 0;
    work_counts[4]++;texture_counts[0]++;
#ifdef GEKKO2_GX_DUMP
    gs_gx_dump_record("TEXTURE_TARGET",bp,bw,x,y);
    gs_gx_dump_record("TEXTURE_STATE",w,h,pipeline?pipeline->blend:0,pipeline?pipeline->z:0);
#endif
    if(surface_enabled&&sample&&pipeline
       &&(gekko2_opt_enabled(GEKKO2_OPT_GX_RESIDENT)||(!pipeline->blend&&!pipeline->zwrite&&(!pipeline->ztest||pipeline->ztst==1u)))
#ifdef GEKKO2_GX_RESIDENT_PIPELINE_DISABLE
       &&!pipeline->blend&&!pipeline->zwrite&&(!pipeline->ztest||pipeline->ztst==1u)
#endif
       ) {
        gs_gx_texture_draw resident_draw;
        if(gs_gx_prepare_texture(&resident_draw,psm,bp,bw,x,y,w,h,columns,rows,step_x,step_y,scanmsk)) {
            if(geometry_kind) {
                if(!gs_gx_prepare_flat(&resident_draw.coverage,geometry_kind,bp,bw,x,y,x+w-1,y+h-1,geometry_xy,0,scanmsk))return 0;
                resident_draw.coverage.psm=psm;
            }
            if(surface_draw_pipeline(&resident_draw,sample,pipeline))return 1;
        }
    }
    if(!gs_mem_sync())return 0;
    if(!gs_gx_prepare_texture(&texture_draw,psm,bp,bw,x,y,w,h,columns,rows,step_x,step_y,scanmsk)||
       texture_draw.coverage.width>efb_width||texture_draw.coverage.height>efb_height||!sample) {
        texture_counts[3]++;return 0;
    }
    if(geometry_kind) {
        if(!gs_gx_prepare_flat(&texture_draw.coverage,geometry_kind,bp,bw,x,y,x+w-1,y+h-1,geometry_xy,0,scanmsk))return 0;
        texture_draw.coverage.psm=psm;
    }
    memset(&texture_draw.pipeline,0,sizeof(texture_draw.pipeline));
    if(pipeline)texture_draw.pipeline=*pipeline;
    uint32_t bytes=source_pack(&texture_draw,sample);
    if(!bytes)return 0;
    pipeline_source_alpha(&texture_draw,texture);
    if(!pipeline_prepare(&texture_draw))return 0;
    if(!readback)readback=memalign(32,GX_PRESENT_TEXTURE);
    if(!readback||!gs_mem_gpu_bind(resolve_capture,NULL))return 0;
    flat_draw=texture_draw.coverage;
    if(texture_draw.hardware_depth)depth_preload(&texture_draw);
    source_upload(bytes);
    GX_SetViewport(0,0,flat_draw.width,flat_draw.height,0,1);GX_SetScissor(0,0,flat_draw.width,flat_draw.height);
    GX_SetPixelFmt(GX_PF_RGB8_Z24,GX_ZC_LINEAR);GX_SetDither(GX_FALSE);GX_SetFieldMode(GX_FALSE,GX_FALSE);
    GX_SetCullMode(GX_CULL_NONE);GX_SetZMode(GX_FALSE,GX_ALWAYS,GX_FALSE);GX_SetZTexture(GX_ZT_DISABLE,GX_TF_Z24X8,0);
    GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);GX_SetColorUpdate(GX_TRUE);GX_SetAlphaUpdate(GX_FALSE);
    GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
    GX_SetNumChans(0);GX_SetNumTexGens(1);GX_SetNumTevStages(1);
    GX_SetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX2x4,GX_TG_TEX0,GX_IDENTITY);
    GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD0,GX_TEXMAP0,GX_COLORNULL);GX_SetTevOp(GX_TEVSTAGE0,GX_REPLACE);
    GX_ClearVtxDesc();GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);GX_SetVtxDesc(GX_VA_TEX0,GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_TEX0,GX_TEX_ST,GX_F32,0);
    Mtx model;Mtx44 projection;guMtxIdentity(model);guOrtho(projection,0,flat_draw.height,0,flat_draw.width,-1,1);
    GX_LoadPosMtxImm(model,GX_PNMTX0);GX_SetCurrentMtx(GX_PNMTX0);GX_LoadProjectionMtx(projection,GX_ORTHOGRAPHIC);
    GXTexObj object;GX_InitTexObj(&object,texture,texture_draw.tw,texture_draw.th,GX_TF_RGBA8,GX_CLAMP,GX_CLAMP,GX_FALSE);
    GX_InitTexObjLOD(&object,GX_NEAR,GX_NEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);GX_LoadTexObj(&object,GX_TEXMAP0);
    uint32_t quads=0;
    for(uint32_t row=0;row<flat_draw.height;) {
        uint32_t end=row+1;while(end<flat_draw.height&&flat_draw.left[end]==flat_draw.left[row]&&flat_draw.right[end]==flat_draw.right[row])end++;
        if(flat_draw.right[row]>flat_draw.left[row])quads++;row=end;
    }
    /* A fixed-coefficient PABE draw splits into complementary late-alpha
     * passes. Low AS copies the source; high AS uses the exact blend program.
     * Alpha rejection precedes depth updates (ZCompLoc FALSE). No extra
     * readback or CPU per-pixel blend/threshold evaluation is required. */
    uint32_t hardware_blend=texture_draw.hardware_blend;
    uint32_t split_pabe=hardware_blend&&texture_draw.pipeline.pabe;
    uint32_t split_z32=texture_draw.hardware_depth==2u;
    uint32_t passes=split_pabe||split_z32?2u:1u;
    for(uint32_t pass=0;pass<passes;pass++) {
        texture_draw.hardware_blend=hardware_blend&&(!split_pabe||pass==1u);
        pipeline_shader(&texture_draw);
        if(split_pabe)GX_SetAlphaCompare(pass?GX_GEQUAL:GX_LESS,128,GX_AOP_AND,GX_ALWAYS,0);
        if(split_z32) {
            GX_SetAlphaCompare(pass?GX_EQUAL:GX_LESS,texture_draw.pipeline.z>>24,GX_AOP_AND,GX_ALWAYS,0);
            GX_SetZMode(GX_TRUE,pass?(texture_draw.pipeline.ztst==2u?GX_GEQUAL:GX_GREATER):GX_ALWAYS,
                texture_draw.pipeline.zwrite?GX_TRUE:GX_FALSE);
        }
        if(quads)GXB(GX_QUADS,GX_VTXFMT0,quads*4u);
        for(uint32_t row=0;row<flat_draw.height;) {
            uint32_t end=row+1;while(end<flat_draw.height&&flat_draw.left[end]==flat_draw.left[row]&&flat_draw.right[end]==flat_draw.right[row])end++;
            if(flat_draw.right[row]>flat_draw.left[row]) {
                float t0=texture_draw.t0+(texture_draw.t1-texture_draw.t0)*(float)row/h;
                float t1=texture_draw.t0+(texture_draw.t1-texture_draw.t0)*(float)end/h;
                float l=flat_draw.left[row],r=flat_draw.right[row];
                float sl=texture_draw.s0+(texture_draw.s1-texture_draw.s0)*l/w;
                float sr=texture_draw.s0+(texture_draw.s1-texture_draw.s0)*r/w;
                pipeline_vertex(l,row,sl,t0,&texture_draw);pipeline_vertex(r,row,sr,t0,&texture_draw);
                pipeline_vertex(r,end,sr,t1,&texture_draw);pipeline_vertex(l,end,sl,t1,&texture_draw);
            }
            row=end;
        }
        if(quads)GX_End();
    }
    texture_draw.hardware_blend=hardware_blend;
    GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
    if(!gs_gx_capture_vram_psmct32(bp,bw,x,y,flat_draw.width,flat_draw.height,0))return 0;
    capture_flat=1;capture_texture=1;work_counts[0]++;work_counts[1]+=quads*passes;work_counts[2]+=capture.bytes;
    texture_counts[1]++;texture_counts[2]+=bytes;
    if(texture_draw.pipeline.ztest||texture_draw.pipeline.zwrite)pipeline_counts[texture_draw.hardware_depth?0:1]++;
    if(texture_draw.pipeline.blend)pipeline_counts[texture_draw.hardware_blend?2:3]++;
    if(split_z32)pipeline_counts[4]++;if(split_pabe)pipeline_counts[5]++;
    GX_SetZTexture(GX_ZT_DISABLE,GX_TF_Z24X8,0);GX_SetZMode(GX_FALSE,GX_ALWAYS,GX_FALSE);return 1;
}
int gs_gx_draw_shaded_triangle(uint32_t psm,uint32_t bp,uint32_t bw,
 int32_t minx,int32_t miny,int32_t maxx,int32_t maxy,const int32_t *xy,
 uint32_t scanmsk,gs_gx_texel_fn sample,const gs_gx_pipeline *pipeline)
{
 shaded_triangle_counts[0]++;
#ifdef GEKKO2_GX_DUMP
 gs_gx_dump_record("SHADED_TARGET",bp,bw,psm,scanmsk);
 gs_gx_dump_record("SHADED_BBOX",minx,miny,maxx,maxy);
#endif
 if(!render_enabled||!initialized||!surface_enabled||!sample||!xy||!pipeline||
    !gekko2_opt_enabled(GEKKO2_OPT_GX_RESIDENT)||psm>1u||scanmsk>=2u||
    maxx<minx||maxy<miny||maxx-minx>=640||maxy-miny>=512)return 0;
 gs_gx_texture_draw d;memset(&d,0,sizeof d);
 if(!gs_gx_prepare_flat(&d.coverage,3,bp,bw,minx,miny,maxx,maxy,xy,0,scanmsk))return 0;
 d.coverage.psm=psm;d.columns=d.coverage.width;d.rows=d.coverage.height;
 d.origin_x=minx;d.origin_y=miny;d.tw=(d.columns+3u)&~3u;d.th=(d.rows+3u)&~3u;
 if(d.columns*d.rows<64u)return 0;
 for(unsigned x=0;x<d.columns;x++)d.column[x]=minx+x;
 for(unsigned y=0;y<d.rows;y++)d.row[y]=miny+y;
 d.s0=0;d.s1=(float)d.columns/d.tw;d.t0=0;d.t1=(float)d.rows/d.th;
 source_key_valid=0;
 if(!surface_draw_pipeline(&d,sample,pipeline))return 0;
 shaded_triangle_counts[1]++;shaded_triangle_counts[2]+=(uint64_t)d.columns*d.rows;
 return 1;
}
int gs_gx_draw_mapped_triangle(uint32_t psm,uint32_t bp,uint32_t bw,int32_t x,int32_t y,uint32_t w,uint32_t h,
 const int32_t *xy,const int32_t *columns,const int32_t *rows,double du,double dv,uint32_t scanmsk,
 gs_gx_texel_fn sample,const gs_gx_pipeline *pipeline)
{
 if(!xy)return 0;geometry_kind=3;memcpy(geometry_xy,xy,sizeof(geometry_xy));
 int result=gs_gx_draw_texture_sprite(psm,bp,bw,x,y,w,h,columns,rows,du,dv,scanmsk,sample,pipeline);
 geometry_kind=0;return result;
}
int gs_gx_draw_gouraud_triangle(uint32_t psm,uint32_t bp,uint32_t bw,
    int32_t minx,int32_t miny,int32_t maxx,int32_t maxy,const int32_t *xy,
    const uint32_t *rgba,uint32_t scanmsk,const gs_gx_pipeline *pipeline)
{
    if(!render_enabled||!initialized||!xy||!rgba||!pipeline||psm!=0u||
       maxx<minx||maxy<miny||maxx-minx>=640||maxy-miny>=512||
       pipeline->ztest||pipeline->zwrite||pipeline->blend)return 0;
    gs_gx_flat_draw d;
    if(!gs_gx_prepare_flat(&d,3,bp,bw,minx,miny,maxx,maxy,xy,0,scanmsk)||
       d.width>efb_width||d.height>efb_height)return 0;
    /* Keep RGB interpolation on GX and defer the readback until a real
     * VRAM consumer. CPU alpha metadata is exact for constant-alpha vertices.
     * Masked scanlines retain the existing coverage/capture fallback. */
    if(surface_enabled && scanmsk<2u &&
       (rgba[0]>>24)==(rgba[1]>>24) && (rgba[0]>>24)==(rgba[2]>>24)) {
        d.rgba=rgba[0];
        if(surface_ensure(&d) && gs_gx_surface_mark(surface,&d)) {
            surface_state(surface->width,surface->height,0);
            GXB(GX_TRIANGLES,GX_VTXFMT0,3);
            for(unsigned i=0;i<3;i++) {
                GX_Position3f32((float)xy[i*2]+0.5f,(float)xy[i*2+1]+0.5f,0);
                GX_Color4u8(rgba[i]&255u,(rgba[i]>>8)&255u,
                           (rgba[i]>>16)&255u,rgba[i]>>24);
            }
            GX_End();surface_texture_valid=0;
            if(!gs_mem_gpu_mark_pending())return 0;
            surface_counts[1]++;surface_counts[6]++;work_counts[0]++;work_counts[1]++;
            return 1;
        }
    }
    if(!gs_mem_sync())return 0;
    if(!readback)readback=memalign(32,GX_PRESENT_TEXTURE);
    if(!readback||!gs_mem_gpu_bind(resolve_capture,NULL))return 0;
    GX_SetViewport(0,0,d.width,d.height,0,1);GX_SetScissor(0,0,d.width,d.height);
    GX_SetPixelFmt(GX_PF_RGB8_Z24,GX_ZC_LINEAR);GX_SetDither(GX_FALSE);
    GX_SetCullMode(GX_CULL_NONE);GX_SetZMode(GX_FALSE,GX_ALWAYS,GX_FALSE);
    GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
    GX_SetColorUpdate(GX_TRUE);GX_SetAlphaUpdate(GX_FALSE);
    GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
    GX_SetNumChans(1);GX_SetNumTexGens(0);GX_SetNumTevStages(1);
    GX_SetChanCtrl(GX_COLOR0A0,GX_FALSE,GX_SRC_VTX,GX_SRC_VTX,0,GX_DF_NONE,GX_AF_NONE);
    GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0,GX_PASSCLR);
    GX_ClearVtxDesc();GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);GX_SetVtxDesc(GX_VA_CLR0,GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_CLR0,GX_CLR_RGBA,GX_RGBA8,0);
    Mtx model;Mtx44 projection;guMtxIdentity(model);guOrtho(projection,0,d.height,0,d.width,-1,1);
    GX_LoadPosMtxImm(model,GX_PNMTX0);GX_SetCurrentMtx(GX_PNMTX0);GX_LoadProjectionMtx(projection,GX_ORTHOGRAPHIC);
    GXB(GX_TRIANGLES,GX_VTXFMT0,3);
    for(unsigned i=0;i<3;i++) {
        /* K: +0.5 -> GX samples pixel centres, GS software samples corners. */
        GX_Position3f32((float)(xy[i*2]-minx)+0.5f,(float)(xy[i*2+1]-miny)+0.5f,0);
        GX_Color4u8(rgba[i]&255u,(rgba[i]>>8)&255u,(rgba[i]>>16)&255u,(rgba[i]>>24)&255u);
    }
    GX_End();
    if(!gs_gx_capture_vram_psmct32(bp,bw,minx,miny,d.width,d.height,rgba[0]>>24))return 0;
    d.rgba=rgba[0]&0xff000000u;capture_flat=1;capture_texture=0;flat_draw=d;work_counts[0]++;work_counts[1]++;work_counts[2]+=capture.bytes;
    return 1;
}
int gs_gx_draw_uv_decal_triangle(uint32_t psm,uint32_t bp,uint32_t bw,
    int32_t minx,int32_t miny,int32_t maxx,int32_t maxy,const int32_t *xy,const float *uv,
    uint32_t tex_w,uint32_t tex_h,uint32_t alpha,uint32_t scanmsk,gs_gx_texel_fn sample)
{
    if(!render_enabled||!initialized||psm!=0u||!xy||!uv||!sample||!tex_w||!tex_h||
       maxx<minx||maxy<miny||maxx-minx>=640||maxy-miny>=512)return 0;
    int32_t cols[640],rows[512];uint32_t w=maxx-minx+1u,h=maxy-miny+1u;
    for(uint32_t x=0;x<w;x++)cols[x]=(int32_t)x;
    for(uint32_t y=0;y<h;y++)rows[y]=(int32_t)y;
    gs_gx_texture_draw d;
    if(!gs_gx_prepare_texture(&d,psm,bp,bw,minx,miny,w,h,cols,rows,1,1,scanmsk)||
       !gs_gx_prepare_flat(&d.coverage,3,bp,bw,minx,miny,maxx,maxy,xy,0,scanmsk)||
       d.coverage.width>efb_width||d.coverage.height>efb_height||!gs_mem_sync())return 0;
    d.coverage.psm=psm;d.origin_x=0;d.origin_y=0;d.tw=(tex_w+3u)&~3u;d.th=(tex_h+3u)&~3u;
    if(d.tw>1024u||d.th>512u)return 0;
    uint32_t bytes=source_pack(&d,sample);if(!bytes)return 0;
    if(!readback)readback=memalign(32,GX_PRESENT_TEXTURE);
    if(!readback||!gs_mem_gpu_bind(resolve_capture,NULL))return 0;
    flat_draw=d.coverage;texture_draw=d;source_upload(bytes);
    GX_SetViewport(0,0,d.coverage.width,d.coverage.height,0,1);GX_SetScissor(0,0,d.coverage.width,d.coverage.height);
    GX_SetPixelFmt(GX_PF_RGB8_Z24,GX_ZC_LINEAR);GX_SetDither(GX_FALSE);GX_SetCullMode(GX_CULL_NONE);
    GX_SetZMode(GX_FALSE,GX_ALWAYS,GX_FALSE);GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
    GX_SetColorUpdate(GX_TRUE);GX_SetAlphaUpdate(GX_FALSE);GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
    GX_SetNumChans(0);GX_SetNumTexGens(1);GX_SetNumTevStages(1);
    GX_SetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX2x4,GX_TG_TEX0,GX_IDENTITY);
    GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD0,GX_TEXMAP0,GX_COLORNULL);GX_SetTevOp(GX_TEVSTAGE0,GX_REPLACE);
    GX_ClearVtxDesc();GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);GX_SetVtxDesc(GX_VA_TEX0,GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_TEX0,GX_TEX_ST,GX_F32,0);
    Mtx model;Mtx44 projection;guMtxIdentity(model);guOrtho(projection,0,d.coverage.height,0,d.coverage.width,-1,1);
    GX_LoadPosMtxImm(model,GX_PNMTX0);GX_SetCurrentMtx(GX_PNMTX0);GX_LoadProjectionMtx(projection,GX_ORTHOGRAPHIC);
    GXTexObj object;GX_InitTexObj(&object,texture,d.tw,d.th,GX_TF_RGBA8,GX_CLAMP,GX_CLAMP,GX_FALSE);
    GX_InitTexObjLOD(&object,GX_NEAR,GX_NEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);GX_LoadTexObj(&object,GX_TEXMAP0);
    GXB(GX_TRIANGLES,GX_VTXFMT0,3);
    for(unsigned i=0;i<3;i++){GX_Position3f32((float)(xy[i*2]-minx),(float)(xy[i*2+1]-miny),0);GX_TexCoord2f32(uv[i*2]/d.tw,uv[i*2+1]/d.th);}
    GX_End();
    if(!gs_gx_capture_vram_psmct32(bp,bw,minx,miny,d.coverage.width,d.coverage.height,alpha))return 0;
    d.coverage.rgba=alpha<<24;flat_draw=d.coverage;capture_flat=1;capture_texture=0;work_counts[0]++;work_counts[1]++;work_counts[2]+=capture.bytes;
    texture_counts[1]++;texture_counts[2]+=bytes;return 1;
}
static uint32_t flat_pipeline_sample(int32_t x,int32_t y){(void)x;(void)y;return geometry_rgba;}
int gs_gx_draw_flat_pipeline(uint32_t psm,uint32_t kind,uint32_t bp,uint32_t bw,
    int32_t minx,int32_t miny,int32_t maxx,int32_t maxy,const int32_t *xy,
    uint32_t rgba,uint32_t scanmsk,const gs_gx_pipeline *pipeline)
{
    if(kind!=3u||!xy||maxx<minx||maxy<miny||maxx-minx>=640||maxy-miny>=512)return 0;
    int32_t columns[640]={0},rows[512]={0};
    geometry_kind=kind;geometry_rgba=rgba;memcpy(geometry_xy,xy,sizeof(geometry_xy));
    int result=gs_gx_draw_texture_sprite(psm,bp,bw,minx,miny,maxx-minx+1,maxy-miny+1,
        columns,rows,0,0,scanmsk,flat_pipeline_sample,pipeline);
    geometry_kind=0;return result;
}
void gs_gx_shutdown(void)
{
    /* GX keeps references to its FIFO. Retain buffers until process exit;
     * never free a bound command FIFO or leave queued texture accesses. */
    gs_gx_set_residency_enabled(0);
    if(initialized)gx_cpu_wait(3);
}
int gs_gx_reset_boot_state(void)
{
#ifdef GEKKO2_GX_DUMP
 gs_gx_dump_reset();gs_gx_dump_record("COLD_BOOT_RESET",0,0,0,0);
#endif
 /* Shutdown temporarily disables residency while draining GX. A cold boot
  * must retain the requested policy, otherwise every later triangle falls
  * back to software for the lifetime of the process. */
 int requested_residency=surface_enabled;
 if(!gs_mem_sync())return 0;
 gs_gx_shutdown();
 if(!gs_mem_gpu_bind(NULL,NULL))return 0;
 if(surface)surface->active=0;
 capture_flat=capture_texture=surface_texture_valid=0;
 source_key_valid=source_cache.valid=source_upload_pending=0;geometry_kind=0;render_enabled=0;
 memset(sync_counts,0,sizeof(sync_counts));memset(surface_counts,0,sizeof(surface_counts));
 memset(resident_pipeline_counts,0,sizeof(resident_pipeline_counts));
 memset(source_cache_counts,0,sizeof(source_cache_counts));
 memset(texture_counts,0,sizeof(texture_counts));memset(pipeline_counts,0,sizeof(pipeline_counts));
 memset(work_counts,0,sizeof(work_counts));gs_mem_reset_sync_stats();
 memset(additive_counts,0,sizeof(additive_counts));
 memset(native_affine_stats,0,sizeof(native_affine_stats));
 surface_enabled=requested_residency;
 return 1;
}
#endif
