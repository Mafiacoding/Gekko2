#include "core/hw/gs_gx_surface.h"
#include "core/hw/gs_mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks, failures, resolves;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL line %d\n", __LINE__); failures++; } } while (0)
static int fail_resolve(void *opaque, uint8_t *vram, uint32_t bytes)
{ (void)opaque; (void)vram; (void)bytes; resolves++; return 0; }
static int ok_resolve(void *opaque, uint8_t *vram, uint32_t bytes)
{ (void)opaque; (void)vram; (void)bytes; resolves++; return 1; }
int main(void)
{
 gs_gx_texture_draw *draw=calloc(1,sizeof(*draw)), *saved=malloc(sizeof(*saved));
 if (!draw || !saved) return 2;
 gs_mem_init();
 draw->coverage.bw=64; draw->coverage.x=3; draw->coverage.y=2;
 draw->coverage.width=8; draw->coverage.height=4; draw->columns=8; draw->rows=2;
 for (unsigned psm_index=0; psm_index<4; psm_index++) {
  unsigned psm=psm_index==3?10:psm_index;
  gs_gx_pipeline p={.zbp=16384,.zpsm=psm,.ztest=1,.zwrite=1,.ztst=2,.z=128};
  for (unsigned mode=0; mode<4; mode++) {
   p.ztst=mode;
   for (unsigned y=0;y<2;y++) {
    draw->coverage.left[y]=1; draw->coverage.right[y]=7;
    for (unsigned x=1;x<7;x++) gs_mem_write_z(p.zbp,64,3+x,2+y,psm,x<2||x>=6?129:128);
   }
   uint32_t lo=0,hi=0; uint64_t tested=99,failed=99;
   CHECK(gs_gx_surface_depth_range(draw,&p,0,8192,&lo,&hi));
   const uint8_t *depth=gs_mem_read_range(lo,hi-lo);
   CHECK(gs_gx_surface_depth_clip(draw,&p,depth,lo,hi-lo,&tested,&failed));
   CHECK(tested==12);
   CHECK(failed==(mode==0||mode==3?12:mode==1?0:4));
   for (unsigned y=0;y<2;y++) {
    CHECK(draw->coverage.left[y]==(mode==1?1:mode==2?2:0));
    CHECK(draw->coverage.right[y]==(mode==1?7:mode==2?6:0));
   }
  }
  /* A disjoint passing island cannot be represented by one span: fail atomically. */
  p.ztst=2;
  for (unsigned y=0;y<2;y++) {
   draw->coverage.left[y]=1; draw->coverage.right[y]=7;
   for (unsigned x=1;x<7;x++) gs_mem_write_z(p.zbp,64,3+x,2+y,psm,x==3?129:127);
  }
  memcpy(saved,draw,sizeof(*draw)); uint32_t lo=0,hi=0; uint64_t tested=99,failed=99;
  CHECK(gs_gx_surface_depth_range(draw,&p,0,8192,&lo,&hi));
  CHECK(!gs_gx_surface_depth_clip(draw,&p,gs_mem_read_range(lo,hi-lo),lo,hi-lo,&tested,&failed));
  CHECK(!memcmp(saved,draw,sizeof(*draw)) && tested==99 && failed==99);
  CHECK(!gs_gx_surface_depth_range(draw,&p,lo,hi,&lo,&hi));
 }
 /* Full unsigned Z32 comparison; Z16 storage truncation does not truncate incoming Z. */
 gs_gx_pipeline p={.zbp=16384,.zpsm=0,.ztest=1,.ztst=3,.z=0x80000000};
 draw->rows=1; draw->coverage.left[0]=1; draw->coverage.right[0]=2;
 gs_mem_write_z(p.zbp,64,4,2,0,0x7fffffff);
 uint32_t lo,hi; uint64_t tested,failed;
 CHECK(gs_gx_surface_depth_range(draw,&p,0,8192,&lo,&hi));
 CHECK(gs_gx_surface_depth_clip(draw,&p,gs_mem_read_range(lo,hi-lo),lo,hi-lo,&tested,&failed));
 CHECK(draw->coverage.left[0]==1 && failed==0);
 /* Nonalias exact Z writes stay resident; alias and raw access respect a failed owner. */
 CHECK(gs_mem_gpu_bind(fail_resolve,NULL)); CHECK(gs_mem_gpu_protect_range(0,8192)); CHECK(gs_mem_gpu_mark_pending());
 resolves=0; gs_mem_write_z(16384,64,0,0,0,0xfeedcafe);
 CHECK(!resolves && gs_mem_gpu_pending()); CHECK(gs_mem_read_z(16384,64,0,0,0)==0xfeedcafe);
 CHECK(gs_mem_read_range(65536,4)!=NULL && !resolves);
 gs_mem_write_z(0,64,0,0,0,0x12345678); CHECK(resolves==1 && gs_mem_gpu_pending());
 CHECK(gs_mem_read_range(6144,4)==NULL && resolves==2);
 CHECK(gs_mem_get()==NULL && resolves==3);
 CHECK(gs_mem_read_range(GS_MEM_SIZE,1)==NULL);
 CHECK(!gs_mem_gpu_bind(ok_resolve,NULL));
 /* Repair the same callback's behaviour is covered by the linked PPC barrier oracle. */
 printf("Resident depth planning, Z formats, transactional holes, and ownership: %u checks, %u failures\n",checks,failures);
 free(draw);free(saved);return !!failures;
}
