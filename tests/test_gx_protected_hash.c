#include "core/hw/gs_mem.h"
#include "core/recompiler/optimization.h"
#include <assert.h>
#include <stdio.h>
static int fail;
static int resolve(void *o,uint8_t *v,uint32_t bytes){(void)o;(void)bytes;v[0]++;return !fail;}
static uint64_t digest(unsigned off){uint64_t h;assert(gs_mem_hash_range(off,8192,&h));return h;}
int main(void){
 gs_mem_init();gekko2_optimization_mask|=1u<<GEKKO2_OPT_GX_SOURCE_CACHE;
 assert(gs_mem_gpu_bind(resolve,0));uint64_t h=digest(32768),hits=gs_mem_hash_cache_hits();
 assert(gs_mem_gpu_protect_range(0,8192));assert(gs_mem_gpu_mark_pending());
 assert(digest(32768)==h&&gs_mem_hash_cache_hits()==hits+1);
 assert(gs_mem_sync());assert(digest(32768)==h&&gs_mem_hash_cache_hits()==hits+2);
 h=digest(0);hits=gs_mem_hash_cache_hits();assert(gs_mem_gpu_protect_range(0,8192));assert(gs_mem_gpu_mark_pending());
 assert(gs_mem_sync());assert(digest(0)!=h&&gs_mem_hash_cache_hits()==hits);
 h=digest(32768);hits=gs_mem_hash_cache_hits();assert(gs_mem_gpu_protect_range(0,8192));assert(gs_mem_gpu_mark_pending());fail=1;
 assert(!gs_mem_sync());assert(digest(32768)==h&&gs_mem_hash_cache_hits()==hits+1);
 fail=0;assert(gs_mem_sync());
 /* Unknown owners invalidate even a physically disjoint previous source. */
 hits=gs_mem_hash_cache_hits();assert(gs_mem_gpu_bind(resolve,0));assert(gs_mem_gpu_mark_pending());assert(gs_mem_sync());digest(32768);assert(gs_mem_hash_cache_hits()==hits);
 puts("PASS protected texture digest: disjoint ownership, overlap, failed and unknown resolvers");
}
