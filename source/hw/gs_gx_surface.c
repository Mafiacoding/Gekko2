/* CPU ownership metadata only. RGB pixel processing belongs to GX. */
#include "core/hw/gs_gx_surface.h"
#include "core/hw/gs_mem.h"
#include <string.h>

static unsigned depth_bytes(uint32_t psm)
{return psm==1u?3u:(psm==2u||psm==10u)?2u:4u;}
int gs_gx_surface_depth_range(const gs_gx_texture_draw *d,const gs_gx_pipeline *p,
 uint32_t color_lo,uint32_t color_hi,uint32_t *lo,uint32_t *hi)
{
 if(!d||!p||!lo||!hi||p->ztst>3u||d->rows>512u||d->columns>640u||
    d->coverage.height>512u||d->coverage.height<d->rows)return 0;
 uint32_t low=UINT32_MAX,high=0;unsigned n=depth_bytes(p->zpsm);
 for(uint32_t y=0;y<d->rows;y++) {
  if(d->coverage.left[y]>d->coverage.right[y]||d->coverage.right[y]>d->columns)return 0;
  for(uint32_t x=d->coverage.left[y];x<d->coverage.right[y];x++) {
   uint32_t off=gs_mem_z_offset(p->zbp,d->coverage.bw,d->coverage.x+x,d->coverage.y+y,p->zpsm);
   if(off==UINT32_MAX)return 0;
   if(off<low)low=off;
   if(off+n>high)high=off+n;
  }
 }
 if(high&&!(high<=color_lo||low>=color_hi))return 0;
 *lo=high?low:0;*hi=high;return 1;
}
int gs_gx_surface_depth_clip(gs_gx_texture_draw *d,const gs_gx_pipeline *p,
 const uint8_t *depth,uint32_t lo,uint32_t bytes,uint64_t *tested,uint64_t *failed)
{
 if(!d||!p||!tested||!failed||d->rows>512u||d->coverage.height>512u||
    d->rows>d->coverage.height||p->ztst>3u)return 0;
 uint16_t left[512],right[512];uint64_t checks=0,rejects=0;
 for(uint32_t y=0;y<d->rows;y++) {
  uint32_t l=d->coverage.left[y],r=d->coverage.right[y],first=r,last=r;int gap=0;
  if(l>r||r>d->columns)return 0;
  for(uint32_t x=l;x<r;x++) {
   uint32_t stored=0;int pass=1;
   if(p->ztest) {
    checks++;
    if(p->ztst>=2u) {
     uint32_t off=gs_mem_z_offset(p->zbp,d->coverage.bw,d->coverage.x+x,d->coverage.y+y,p->zpsm);
     unsigned n=depth_bytes(p->zpsm);
     if(!depth||off==UINT32_MAX||off<lo||off-lo>bytes||n>bytes-(off-lo))return 0;
     for(unsigned k=0;k<n;k++)stored|=(uint32_t)depth[off-lo+k]<<(8*k);
    }
    pass=p->ztst==1u||(p->ztst==2u?p->z>=stored:p->ztst==3u?p->z>stored:0);
   }
   if(pass) {
    if(gap)return 0; /* Existing compact path handles arbitrary holes. */
    if(first==r)first=x;
    last=x+1u;
   } else {rejects++;if(first!=r)gap=1;}
  }
  left[y]=first==r?0:first;right[y]=first==r?0:last;
 }
 /* Commit only after every row proved representable. */
 memcpy(d->coverage.left,left,d->rows*sizeof(*left));
 memcpy(d->coverage.right,right,d->rows*sizeof(*right));
 *tested=checks;*failed=rejects;return 1;
}
static uint32_t offset(uint32_t bp,uint32_t bw,uint32_t x,uint32_t y)
{
 uint32_t bx=(x&63)>>3,by=(y&31)>>3;
 uint32_t block=(bx&1)|((by&1)<<1)|((bx&2)<<1)|((by&2)<<2)|((bx&4)<<2);
 uint32_t word=(x&1)|((y&1)<<1)|((x&6)<<1)|((y&6)<<3);
 return bp*4+((y/32)*(bw/64)+x/64)*8192+block*256+word*4;
}
int gs_gx_surface_begin(gs_gx_surface *s,uint32_t bp,uint32_t bw,uint32_t width,uint32_t height)
{
 if(!s||width!=bw||!gs_gx_target_valid(GS_MEM_SIZE,bp,bw,0,0,width,height))return 0;
 s->bp=bp;s->bw=bw;s->width=width;s->height=height;s->active=1;s->dirty_pixels=0;
 memset(s->dirty,0,width*height);return 1;
}
int gs_gx_surface_mark(gs_gx_surface *s,const gs_gx_flat_draw *d)
{
 if(!s||!d||!s->active||d->psm>1||d->bp!=s->bp||d->bw!=s->bw||
    !d->width||!d->height||d->x>s->width||d->width>s->width-d->x||
    d->y>s->height||d->height>s->height-d->y)return 0;
 for(uint32_t y=0;y<d->height;y++)if(d->left[y]>d->right[y]||d->right[y]>d->width)return 0;
 for(uint32_t y=0;y<d->height;y++)for(uint32_t x=d->left[y];x<d->right[y];x++) {
  uint32_t i=(d->y+y)*s->width+d->x+x;
  if(!s->dirty[i])s->dirty_pixels++;
  s->dirty[i]|=1;
  if(!d->psm){s->dirty[i]|=2;s->alpha[i]=d->rgba>>24;}
 }
 return 1;
}
int gs_gx_surface_import(gs_gx_surface *s,uint8_t *vram,uint32_t size,const void *pixels,uint32_t capacity)
{
 if(!s||!s->active||!vram||!pixels||s->width!=s->bw||
    !gs_gx_target_valid(size,s->bp,s->bw,0,0,s->width,s->height)||capacity<s->width*s->height*4)return 0;
 const uint8_t *p=pixels;
 for(uint32_t y=0;y<s->height;y++)for(uint32_t x=0;x<s->width;x++) {
  uint32_t i=y*s->width+x;if(!s->dirty[i])continue;
  uint32_t o=offset(s->bp,s->bw,x,y),t=((y/4)*(s->width/4)+x/4)*64+(y&3)*8+(x&3)*2;
  vram[o]=p[t+1];vram[o+1]=p[t+32];vram[o+2]=p[t+33];
  if(s->dirty[i]&2)vram[o+3]=s->alpha[i];
 }
 s->active=0;return 1;
}
