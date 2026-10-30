/*
 * gs_wii_output.c - see include/core/hw/gs_wii_output.h.
 */

#include "core/hw/gs_wii_output.h"
#include "core/hw/gs_mem.h"
#include <stddef.h>
#include <limits.h>

uint32_t gs_rgb8_pair_to_ycbcr(uint8_t r1, uint8_t g1, uint8_t b1,
                                uint8_t r2, uint8_t g2, uint8_t b2)
{
    /* Ported verbatim from libogc's console.c RGB8_to_YCbCr (BSD-style
     * license). The 16/240 clamping matches broadcast-safe YCbCr
     * range, not full 0-255 range - this is intentional and matches
     * what the Wii's video hardware/libogc itself does. */
    if (r1 < 16) r1 = 16;
    if (g1 < 16) g1 = 16;
    if (b1 < 16) b1 = 16;
    if (r2 < 16) r2 = 16;
    if (g2 < 16) g2 = 16;
    if (b2 < 16) b2 = 16;

    if (r1 > 240) r1 = 240;
    if (g1 > 240) g1 = 240;
    if (b1 > 240) b1 = 240;
    if (r2 > 240) r2 = 240;
    if (g2 > 240) g2 = 240;
    if (b2 > 240) b2 = 240;

    uint8_t Y1 = (uint8_t)((77 * r1 + 150 * g1 + 29 * b1) / 256);
    uint8_t Y2 = (uint8_t)((77 * r2 + 150 * g2 + 29 * b2) / 256);
    uint8_t Cb = (uint8_t)((112 * (b1 + b2) -  74 * (g1 + g2) - 38 * (r1 + r2)) / 512 + 128);
    uint8_t Cr = (uint8_t)((112 * (r1 + r2) - 94  * (g1 + g2) - 18 * (b1 + b2)) / 512 + 128);

    return ((uint32_t)Y1 << 24) | ((uint32_t)Cb << 16) | ((uint32_t)Y2 << 8) | (uint32_t)Cr;
}

static inline void rgba32_to_rgb8(uint32_t rgba, uint8_t *r, uint8_t *g, uint8_t *b)
{
    /* PSMCT32 channel order as stored by gs_mem_write_psmct32's caller
     * convention: 0xAABBGGRR (matches how we pack test/demo colors
     * elsewhere in this project - see tests). */
    *r = (uint8_t)(rgba & 0xFF);
    *g = (uint8_t)((rgba >> 8) & 0xFF);
    *b = (uint8_t)((rgba >> 16) & 0xFF);
}

/* Round 119 (task #172/#274) - see gs_wii_output.h's doc comment.
 * Moved verbatim from main.c's decode_dispfb() (task #126); logic is
 * byte-for-byte identical, only the name/location changed (renamed
 * with a gs_ prefix to match this file's own naming convention). */
void gs_decode_dispfb(uint64_t dispfb, uint32_t *out_bp_words, uint32_t *out_bw_pixels)
{
    uint32_t fbp_field = (uint32_t)(dispfb & 0x1FFu);
    uint32_t fbw_field  = (uint32_t)((dispfb >> 9) & 0x3Fu);
    *out_bp_words  = fbp_field * 2048u;
    *out_bw_pixels = fbw_field * 64u;
}

void gs_blit_psmct32_to_xfb(void *xfb, uint32_t xfb_width_px,
                             uint32_t dst_x, uint32_t dst_y,
                             uint32_t gs_bp, uint32_t gs_bw,
                             uint32_t src_x, uint32_t src_y,
                             uint32_t width, uint32_t height)
{
    uint32_t *xfb32 = (uint32_t *)xfb;
    uint32_t xfb_words_per_row = xfb_width_px / 2;

    for (uint32_t row = 0; row < height; row++) {
        for (uint32_t col = 0; col < width; col += 2) {
            uint32_t p1 = gs_mem_read_psmct32(gs_bp, gs_bw, src_x + col, src_y + row);
            uint32_t p2 = (col + 1 < width)
                ? gs_mem_read_psmct32(gs_bp, gs_bw, src_x + col + 1, src_y + row)
                : p1; /* odd trailing pixel: duplicate so the pair conversion still makes sense */

            uint8_t r1, g1, b1, r2, g2, b2;
            rgba32_to_rgb8(p1, &r1, &g1, &b1);
            rgba32_to_rgb8(p2, &r2, &g2, &b2);

            uint32_t yuv = gs_rgb8_pair_to_ycbcr(r1, g1, b1, r2, g2, b2);

            uint32_t dst_word_x = (dst_x + col) / 2;
            uint32_t dst_row = dst_y + row;
            xfb32[dst_row * xfb_words_per_row + dst_word_x] = yuv;
        }
    }
}

void gs_decode_display_region(uint64_t dispfb, uint64_t display, uint64_t smode2,
                              uint32_t *x, uint32_t *y, uint32_t *w, uint32_t *h)
{
    *x = (uint32_t)(dispfb >> 32) & 0x7ffu;
    *y = (uint32_t)(dispfb >> 48) & 0x7ffu;
    *w = (((uint32_t)(display >> 32) & 0xfffu) + 1u) /
         (((uint32_t)(display >> 23) & 15u) + 1u);
    *h = (((uint32_t)(display >> 44) & 0x7ffu) + 1u) /
         (((uint32_t)(display >> 27) & 3u) + 1u);
    /* PCSX2 GSState framebufferRect: INT+FFMD reads half the height. */
    if ((smode2 & 3u) == 3u) *h = (*h + 1u) / 2u;
}

/* DISPFB.PSM applies to the *scanout source*, not necessarily to the
 * active FRAME drawing format. The former 32-bit-only path decoded CT16
 * scanout as arbitrary 32-bit VRAM and could incorrectly gate FIRST_IMAGE.
 * Preserve the exact 32/24-bit path, but unpack 5:5:5:1 pixels for CT16 and
 * CT16S using their distinct physical page layouts. No guest VRAM mutation. */
uint32_t gs_scanout_read_rgb(uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,uint32_t psm)
{
    uint32_t v;
    if(psm==0u||psm==1u)return gs_mem_read_psmct32(bp,bw,x,y)&0x00ffffffu;
    if(psm==2u)v=gs_mem_read_psmct16(bp,bw,x,y);
    else if(psm==10u)v=gs_mem_read_psmct16s(bp,bw,x,y);
    else return 0;
    uint32_t rr=v&31u,gg=(v>>5)&31u,bb=(v>>10)&31u;
    return ((rr<<3)|(rr>>2)) | (((gg<<3)|(gg>>2))<<8) |
           (((bb<<3)|(bb>>2))<<16);
}

/* Presentation filtering for the interlaced GS field. Cache converted
 * source lines, then interpolate Y/Cb/Cr vertically. GS textures and RAM
 * remain untouched; horizontal sampling retains the previous convention. */
static void gs_scaled_line(uint32_t *out,uint32_t dw,uint32_t bp,uint32_t bw,
                           uint32_t sx,uint32_t sy,uint32_t sw,uint32_t psm)
{
    /* Native-width scanout is the normal BIOS path. Hoist swizzle row
     * arithmetic and decode one contiguous line, retaining exact colors. */
    if(dw==sw && dw<=1024u) {
        uint32_t rgba[1024];
        if(psm<=1u)gs_mem_read_psmct32_span(rgba,bp,bw,sx,sy,dw);
        else for(uint32_t x=0;x<dw;x++)rgba[x]=gs_scanout_read_rgb(bp,bw,sx+x,sy,psm);
        for(uint32_t x=0;x<dw;x+=2u) {
            uint32_t a=rgba[x],b=rgba[x+1u];
            out[x/2u]=gs_rgb8_pair_to_ycbcr(a,a>>8,a>>16,b,b>>8,b>>16);
        }
        return;
    }
    uint32_t fx=0,step=(sw<<16)/dw;
    for(uint32_t x=0;x<dw;x+=2) {
        uint32_t a=gs_scanout_read_rgb(bp,bw,sx+(fx>>16),sy,psm);fx+=step;
        uint32_t b=gs_scanout_read_rgb(bp,bw,sx+(fx>>16),sy,psm);fx+=step;
        out[x/2]=gs_rgb8_pair_to_ycbcr(a,a>>8,a>>16,b,b>>8,b>>16);
    }
}
int gs_blit_scaled_psm_to_xfb_checked(void *xfb,uint32_t dst_w,uint32_t dst_h,
                                uint32_t bp,uint32_t bw,uint32_t sx,uint32_t sy,
                                uint32_t sw,uint32_t sh,uint32_t psm)
{
    if(!xfb||!dst_w||!dst_h||!bw||!sw||!sh||(dst_w&1u)||
       sw>4096u||sh>2048u||sx>UINT32_MAX-sw||sy>UINT32_MAX-sh||
       !(psm==0u||psm==1u||psm==2u||psm==10u))return 0;
    /* A failed deferred readback returns zero from individual pixel reads.
     * Resolve before the first XFB write so those zeros cannot overwrite
     * the last valid image or be reported as a successful black scanout. */
    if(!gs_mem_sync())return -1;
    uint32_t *out=(uint32_t*)xfb;
    /* Bounded 4KB line cache; unusual wider outputs retain the old path. */
    if(dst_w>1024u) {
        uint32_t fy=0,step=(sh<<16)/dst_h;
        for(uint32_t y=0;y<dst_h;y++,fy+=step)
            gs_scaled_line(out+y*(dst_w/2),dst_w,bp,bw,sx,sy+(fy>>16),sw,psm);
        return 1;
    }
    uint32_t rows[2][512],*a=rows[0],*b=rows[1];
    uint32_t ka=~0u,kb=~0u,step=(sh<<16)/dst_h;
    int64_t fy=((int64_t)step-65536)/2;
    for(uint32_t y=0;y<dst_h;y++,fy+=step) {
        uint32_t row=fy<0?0u:(uint32_t)(fy>>16);
        uint32_t f=fy<0?0u:(uint32_t)fy&65535u;
        if(row>=sh-1u){row=sh-1u;f=0;}
        uint32_t next=f?row+1u:row;
        if(kb==row){uint32_t *t=a;a=b;b=t;uint32_t k=ka;ka=kb;kb=k;}
        if(ka!=row){gs_scaled_line(a,dst_w,bp,bw,sx,sy+row,sw,psm);ka=row;}
        if(f&&kb!=next){gs_scaled_line(b,dst_w,bp,bw,sx,sy+next,sw,psm);kb=next;}
        for(uint32_t x=0;x<dst_w/2u;x++) {
            uint32_t v=a[x];
            if(f){v=0;for(unsigned c=0;c<32;c+=8)
                v|=(((((a[x]>>c)&255u)*(65536u-f)+((b[x]>>c)&255u)*f+32768u)>>16)&255u)<<c;}
            out[y*(dst_w/2u)+x]=v;
        }
    }
    return 1;
}

void gs_blit_scaled_psm_to_xfb(void *xfb,uint32_t dst_w,uint32_t dst_h,
                                uint32_t bp,uint32_t bw,uint32_t sx,uint32_t sy,
                                uint32_t sw,uint32_t sh,uint32_t psm)
{
    (void)gs_blit_scaled_psm_to_xfb_checked(xfb,dst_w,dst_h,bp,bw,sx,sy,sw,sh,psm);
}

void gs_blit_scaled_psmct32_to_xfb(void *xfb,uint32_t dst_w,uint32_t dst_h,
                                  uint32_t bp,uint32_t bw,uint32_t sx,uint32_t sy,
                                  uint32_t sw,uint32_t sh)
{gs_blit_scaled_psm_to_xfb(xfb,dst_w,dst_h,bp,bw,sx,sy,sw,sh,0u);}

int gs_display_has_rgb_psm(uint32_t bp,uint32_t bw,uint32_t sx,uint32_t sy,uint32_t sw,uint32_t sh,uint32_t psm)
{
    if(!bw||!sw||!sh||!(psm==0u||psm==1u||psm==2u||psm==10u))return 0;
    for(uint32_t y=0;y<16;y++)for(uint32_t x=0;x<32;x++)
        if(gs_scanout_read_rgb(bp,bw,sx+(uint64_t)x*sw/32,sy+(uint64_t)y*sh/16,psm))return 1;
    /* A black sample grid is not proof of a black display: thin text or
     * sparse BIOS pixels can fall between all 512 samples and leave GX
     * permanently behind the first-image gate. Check the complete active
     * region with bounded stack storage and hoisted swizzle row reads.
     * main.c calls this only before the first confirmed image. */
    if(sw>4096u||sh>2048u||sx>UINT32_MAX-sw||sy>UINT32_MAX-sh)return 0;
    uint32_t pixels[64];
    for(uint32_t y=0;y<sh;y++)for(uint32_t x=0;x<sw;) {
        uint32_t count=sw-x;if(count>64u)count=64u;
        if(psm<=1u)gs_mem_read_psmct32_span(pixels,bp,bw,sx+x,sy+y,count);
        else for(uint32_t i=0;i<count;i++)pixels[i]=gs_scanout_read_rgb(bp,bw,sx+x+i,sy+y,psm);
        for(uint32_t n=0;n<count;n++)if(pixels[n]&0xffffffu)return 1;
        x+=count;
    }
    return 0;
}

int gs_display_has_rgb(uint32_t bp,uint32_t bw,uint32_t sx,uint32_t sy,uint32_t sw,uint32_t sh)
{return gs_display_has_rgb_psm(bp,bw,sx,sy,sw,sh,0u);}

void gs_scanout_probe_rgb_psm(uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint32_t psm,uint32_t stats[6])
{
 for(unsigned i=0;i<6;i++)stats[i]=0;
 if(!w||!h||!bw||w>2048u||h>2048u||x>2048u-w||y>2048u-h||
    !(psm==0u||psm==1u||psm==2u||psm==10u))return;
 for(unsigned j=0;j<8;j++)for(unsigned i=0;i<8;i++){
  uint32_t c=gs_scanout_read_rgb(bp,bw,x+(w-1u)*i/7u,y+(h-1u)*j/7u,psm);
  uint32_t r=c&255u,g=(c>>8)&255u,b=(c>>16)&255u,l=(77*r+150*g+29*b)>>8;
  stats[0]++;stats[1]+=r;stats[2]+=g;stats[3]+=b;if(l>stats[4])stats[4]=l;stats[5]+=!!(c&0xffffffu);
 }
 stats[1]/=stats[0];stats[2]/=stats[0];stats[3]/=stats[0];
}

void gs_scanout_probe_rgb(uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint32_t stats[6])
{gs_scanout_probe_rgb_psm(bp,bw,x,y,w,h,0u,stats);}

int gs_scanout_grid_hash(uint32_t bp,uint32_t bw,uint32_t x,uint32_t y,
                         uint32_t w,uint32_t h,uint32_t psm,uint32_t *hash)
{
    *hash=0;
    if(!bw||!w||!h||w>2048u||h>2048u||x>2048u-w||y>2048u-h||
       !(psm==0u||psm==1u||psm==2u||psm==10u))return 0;
    uint32_t failures=gs_mem_sync_failures(),v=2166136261u;
    for(unsigned j=0;j<8;j++)for(unsigned i=0;i<8;i++) {
        uint32_t c=gs_scanout_read_rgb(bp,bw,x+(w-1u)*i/7u,y+(h-1u)*j/7u,psm);
        if(gs_mem_sync_failures()!=failures)return 0;
        /* Explicit channel order makes fingerprints host-endian independent. */
        for(unsigned b=0;b<24;b+=8)v=(v^((c>>b)&255u))*16777619u;
    }
    *hash=v;
    return 1;
}

/* Presentation only: GS VRAM, DISPLAY and emulated resolution are untouched.
 * Work in YCbCr pairs to preserve 4:2:2 chroma grouping and row bounds. */
int gs_xfb_fit_4_3(void *xfb,uint32_t width,uint32_t height,int widescreen)
{
    if(!widescreen)return 1;
    if(!xfb||width<8u||width>1024u||(width&7u)||!height)return 0;
    uint32_t line[512],pairs=width/2u,active=pairs*3u/4u,left=(pairs-active)/2u;
    uint32_t *pixels=xfb;
    for(uint32_t y=0;y<height;y++) {
        uint32_t *row=pixels+(size_t)y*pairs;
        for(uint32_t x=0;x<pairs;x++)line[x]=row[x];
        for(uint32_t x=0;x<pairs;x++)row[x]=0x10801080u;
        for(uint32_t x=0;x<active;x++)row[left+x]=line[((2u*x+1u)*pairs)/(2u*active)];
    }
    return 1;
}

int gs_output_requires_merge(const gs_state_t *g)
{
 if(!g)return 0;
 unsigned enabled=(unsigned)g->pmode&3u;
 if(enabled==2u)return !!(g->pmode&128u);
 if(enabled==1u&&(g->pmode&32u)&&((g->pmode>>8)&255u)==255u)return 0;
 return enabled!=0u;
}
uint64_t gs_output_state_key(const gs_state_t *g)
{
 const uint64_t values[]={g->pmode,g->smode2,g->dispfb1,g->display1,g->dispfb2,g->display2,g->bgcolor};
 uint64_t hash=1469598103934665603ull;
 for(unsigned n=0;n<sizeof values/sizeof *values;n++)for(unsigned b=0;b<64;b+=8)
  hash=(hash^((values[n]>>b)&255u))*1099511628211ull;
 return hash;
}
int gs_output_make_plan(const gs_state_t *g,gs_output_plan *out)
{
 if(!g||!out||!(g->pmode&3u))return 0;
 gs_output_plan p={0};p.pmode=g->pmode;p.bgcolor=g->bgcolor;
 uint32_t minx=UINT32_MAX,miny=UINT32_MAX;
 for(unsigned i=0;i<2;i++){
  gs_output_circuit *c=&p.circuit[i];c->enabled=(uint32_t)(g->pmode>>i)&1u;
  if(!c->enabled)continue;
  uint64_t fb=i?g->dispfb2:g->dispfb1,display=i?g->display2:g->display1;
  gs_decode_dispfb(fb,&c->bp,&c->bw);c->psm=(uint32_t)(fb>>15)&31u;
  gs_decode_display_region(fb,display,g->smode2,&c->sx,&c->sy,&c->width,&c->height);
  if(!c->bw||!c->width||!c->height||c->width>4096u||c->height>2048u||
     !(c->psm==0u||c->psm==1u||c->psm==2u||c->psm==10u))return 0;
  c->dx=((uint32_t)display&4095u)/(((uint32_t)(display>>23)&15u)+1u);
  c->dy=((uint32_t)(display>>12)&2047u)/(((uint32_t)(display>>27)&3u)+1u);
  if((g->smode2&3u)==3u)c->dy/=2u;
  if(c->dx<minx)minx=c->dx;if(c->dy<miny)miny=c->dy;
 }
 for(unsigned i=0;i<2;i++){
  gs_output_circuit *c=&p.circuit[i];if(!c->enabled)continue;
  c->dx-=minx;c->dy-=miny;
  if(c->dx+c->width>p.width)p.width=c->dx+c->width;
  if(c->dy+c->height>p.height)p.height=c->dy+c->height;
 }
 if(p.width>8192u||p.height>4096u)return 0;
 *out=p;return 1;
}
static uint32_t output_circuit_read(const gs_output_circuit *c,uint32_t x,uint32_t y)
{
 x=c->sx+x-c->dx;y=c->sy+y-c->dy;
 if(c->psm==0u)return gs_mem_read_psmct32(c->bp,c->bw,x,y);
 if(c->psm==1u)return (gs_mem_read_psmct32(c->bp,c->bw,x,y)&0xffffffu)|0x80000000u;
 uint32_t v=c->psm==2u?gs_mem_read_psmct16(c->bp,c->bw,x,y):gs_mem_read_psmct16s(c->bp,c->bw,x,y);
 unsigned r=v&31u,g=(v>>5)&31u,b=(v>>10)&31u;
 return (r<<3)|(r>>2)|(((g<<3)|(g>>2))<<8)|(((b<<3)|(b>>2))<<16)|((v&32768u)?0x80000000u:0);
}
static int circuit_contains(const gs_output_circuit *c,uint32_t x,uint32_t y)
{return c->enabled&&x>=c->dx&&y>=c->dy&&x-c->dx<c->width&&y-c->dy<c->height;}
uint32_t gs_output_read_rgba(const gs_output_plan *p,uint32_t x,uint32_t y)
{
 if(!p||x>=p->width||y>=p->height)return 0;
 uint32_t bottom=((uint32_t)p->bgcolor&0xffffffu)|((uint32_t)(p->pmode>>8)&255u)<<24;
 if(!(p->pmode&128u)&&circuit_contains(&p->circuit[1],x,y))bottom=output_circuit_read(&p->circuit[1],x,y);
 if(!circuit_contains(&p->circuit[0],x,y))return bottom;
 uint32_t top=output_circuit_read(&p->circuit[0],x,y);
 /* PCSX2 merge shader: constant ALP is UNORM8; otherwise twice source
  * alpha, saturated by the blend unit. GS draw ALPHA uses a different
  * coefficient convention. RGB integer rounding remains explicit here. */
 unsigned alpha=(p->pmode&32u)?(unsigned)(p->pmode>>8)&255u:(top>>24)*2u;
 if(alpha>255u)alpha=255u;
 uint32_t result=(p->pmode&64u)?bottom&0xff000000u:top&0xff000000u;
 for(unsigned shift=0;shift<24;shift+=8){
  unsigned a=(top>>shift)&255u,b=(bottom>>shift)&255u;
  result|=((a*alpha+b*(255u-alpha)+127u)/255u)<<shift;
 }
 return result;
}
int gs_output_has_rgb(const gs_state_t *g)
{
 gs_output_plan p;if(!gs_output_make_plan(g,&p)||!gs_mem_sync())return 0;
 for(unsigned y=0;y<16;y++)for(unsigned x=0;x<32;x++)
  if(gs_output_read_rgba(&p,(uint32_t)((uint64_t)x*p.width/32),(uint32_t)((uint64_t)y*p.height/16))&0xffffffu)return 1;
 for(uint32_t y=0;y<p.height;y++)for(uint32_t x=0;x<p.width;x++)
  if(gs_output_read_rgba(&p,x,y)&0xffffffu)return 1;
 return 0;
}
static void output_scaled_line(uint32_t *out,uint32_t width,const gs_output_plan *p,uint32_t y)
{
 for(uint32_t x=0;x<width;x+=2){
  uint32_t a=gs_output_read_rgba(p,(uint32_t)((uint64_t)x*p->width/width),y);
  uint32_t b=gs_output_read_rgba(p,(uint32_t)((uint64_t)(x+1u)*p->width/width),y);
  out[x/2]=gs_rgb8_pair_to_ycbcr(a,a>>8,a>>16,b,b>>8,b>>16);
 }
}
int gs_output_present_checked(const gs_state_t *g,void *xfb,uint32_t width,uint32_t height)
{
 gs_output_plan p;
 if(!xfb||!width||width>1024u||(width&1u)||!height||height>2048u||!gs_output_make_plan(g,&p))return 0;
 if(!gs_mem_sync())return -1; /* Leave the complete old XFB untouched. */
 uint32_t rows[2][512],*a=rows[0],*b=rows[1],ka=~0u,kb=~0u;
 uint32_t step=(p.height<<16)/height;int64_t fy=((int64_t)step-65536)/2;
 for(uint32_t y=0;y<height;y++,fy+=step){
  uint32_t row=fy<0?0u:(uint32_t)(fy>>16),f=fy<0?0u:(uint32_t)fy&65535u;
  if(row>=p.height-1u){row=p.height-1u;f=0;}
  if(kb==row){uint32_t *t=a;a=b;b=t;uint32_t k=ka;ka=kb;kb=k;}
  if(ka!=row){output_scaled_line(a,width,&p,row);ka=row;}
  if(f&&kb!=row+1u){output_scaled_line(b,width,&p,row+1u);kb=row+1u;}
  for(uint32_t x=0;x<width/2u;x++){
   uint32_t v=a[x];if(f){v=0;for(unsigned s=0;s<32;s+=8)
    v|=(((((a[x]>>s)&255u)*(65536u-f)+((b[x]>>s)&255u)*f+32768u)>>16)&255u)<<s;}
   ((uint32_t*)xfb)[(size_t)y*(width/2u)+x]=v;
  }
 }
 return 1;
}
