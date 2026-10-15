#include "core/recompiler/ppc_code_cache.h"
#include "core/recompiler/optimization.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
uint32_t gekko2_optimization_mask=GEKKO2_OPT_DEFAULT;
int main(void)
{
 ppc_code_cache_info a,b;ppc_code_cache_get_info(&a);
 assert(a.free_bytes==PPC_CODE_CACHE_BYTES&&a.free_spans==1&&!a.live_spans);
 assert(ppc_code_cache_can_alloc(4096));ppc_code_cache_get_info(&a);
 void *p=ppc_code_cache_alloc(4096);assert(p);memset(p,0x37,4096);ppc_code_cache_get_info(&b);
 assert(b.scan_steps==a.scan_steps&&b.admission_reuses==a.admission_reuses+1);
 assert(b.live_spans==1&&b.allocations==1);
 /* A cached failure must become a success when an owner is released. */
 void *q=ppc_code_cache_alloc(PPC_CODE_CACHE_BYTES-b.used-32);assert(q);
 assert(!ppc_code_cache_can_alloc(32));assert(!ppc_code_cache_alloc(32));
 ppc_code_cache_release(p);assert(ppc_code_cache_can_alloc(4096));
 p=ppc_code_cache_alloc(4096);assert(p);memset(p,0x55,4096);
 assert(ppc_code_cache_can_alloc(32)==0);
 ppc_code_cache_trim(p,96);assert(ppc_code_cache_can_alloc(256));
 void *r=ppc_code_cache_alloc(256);assert(r);
 for(unsigned n=0;n<96;n++)assert(((unsigned char*)p)[n]==0x55);
 ppc_code_cache_release(p);ppc_code_cache_release(q);ppc_code_cache_release(r);
 ppc_code_cache_get_info(&a);assert(!a.used&&!a.live_spans&&a.free_spans==1&&a.allocations==a.releases);
 assert(a.largest_free_payload==PPC_CODE_CACHE_BYTES-32);
 puts("PASS admission reuse, failed-probe invalidation, trim invalidation, stable owners and balanced allocation counters");
}
