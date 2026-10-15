#include "core/hw/gs_mem.h"
#include "core/recompiler/optimization.h"
#include <stdio.h>
#include <string.h>
static int failures,resolve_fail;
#define CHECK(x) do{if(!(x)){printf("FAIL line %d\n",__LINE__);failures++;}}while(0)
static int resolve(void *o,uint8_t *mem,uint32_t size){(void)o;(void)size;mem[0]++;return !resolve_fail;}
static uint64_t hash(uint32_t off,uint32_t bytes){uint64_t h=0;CHECK(gs_mem_hash_range(off,bytes,&h));return h;}
static void changed(uint64_t old){CHECK(hash(0,8192)!=old);uint64_t before=gs_mem_hash_cache_hits();hash(0,8192);CHECK(gs_mem_hash_cache_hits()==before+1);}
int main(void){
 gekko2_optimization_mask|=1u<<GEKKO2_OPT_GX_SOURCE_CACHE;gs_mem_init();
 uint64_t h=hash(0,8192);CHECK(hash(0,8192)==h);CHECK(gs_mem_hash_cache_hits()==1&&gs_mem_hash_cache_bytes_avoided()==8192);
 gs_mem_write_psmct32(8192,64,0,0,0xabcdef01);CHECK(hash(0,8192)==h);CHECK(gs_mem_hash_cache_hits()==2);
 gs_mem_write_psmct32(0,64,0,0,1);changed(h);h=hash(0,8192);
 gs_mem_write_psmct32_swizzled(0,64,1,0,2);changed(h);h=hash(0,8192);
 gs_mem_write_psmct16(0,64,0,0,3);changed(h);h=hash(0,8192);
 gs_mem_write_psmct16s(0,64,0,0,4);changed(h);h=hash(0,8192);
 gs_mem_write_index(0,128,0,0,0x13,5);changed(h);h=hash(0,8192);
 gs_mem_write_index(0,128,0,0,0x14,6);changed(h);h=hash(0,8192);
 gs_mem_write_z(0,64,0,0,0,7);changed(h);h=hash(0,8192);
 gs_mem_fill_psmct32_span(0,64,0,0,64,0x87654321);changed(h);
 CHECK(gs_mem_gpu_bind(resolve,0));h=hash(0,8192);CHECK(gs_mem_gpu_mark_pending());changed(h);
 h=hash(0,8192);resolve_fail=1;CHECK(gs_mem_gpu_mark_pending());uint64_t value;CHECK(!gs_mem_hash_range(0,8192,&value));CHECK(gs_mem_gpu_pending());resolve_fail=0;changed(h);
 uint64_t hits=gs_mem_hash_cache_hits();gekko2_optimization_mask&=~(1u<<GEKKO2_OPT_GX_SOURCE_CACHE);hash(0,8192);hash(0,8192);CHECK(gs_mem_hash_cache_hits()==hits);gs_mem_write_psmct32(0,64,0,0,33);gekko2_optimization_mask|=1u<<GEKKO2_OPT_GX_SOURCE_CACHE;hash(0,8192);CHECK(gs_mem_hash_cache_hits()==hits);
 h=hash(0,8192);uint8_t patch[2]={0x91,0x82};CHECK(gs_mem_write_range(0,patch,2));changed(h);CHECK(!gs_mem_write_range(GS_MEM_SIZE,patch,2));CHECK(!gs_mem_write_range(0,0,1));
 CHECK(!gs_mem_hash_range(GS_MEM_SIZE,1,&value));CHECK(!gs_mem_hash_range(0,1,0));
 h=hash(0,8192);uint8_t *raw=gs_mem_get();CHECK(raw);raw[0]++;CHECK(hash(0,8192)!=h);hits=gs_mem_hash_cache_hits();hash(0,8192);CHECK(gs_mem_hash_cache_hits()==hits);
 gs_mem_init();hash(0,8192);raw[0]++;h=hash(0,8192);CHECK(h!=hash(8192,8192));CHECK(gs_mem_hash_cache_hits()==0);
 printf("GS digest reuse: all pixel formats, disjoint writes, GPU failure, option toggle and escaped raw pointer: %d failures\n",failures);return !!failures;
}
