#include "core/recompiler/ppc_code_cache.h"
#include "core/recompiler/optimization.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
uint32_t gekko2_optimization_mask=GEKKO2_OPT_DEFAULT;
int main(void)
{
 assert(ppc_code_cache_capacity()==6u*1024u*1024u);
 void *a=ppc_code_cache_alloc(4096),*b=ppc_code_cache_alloc(777),*c=ppc_code_cache_alloc(8192);
 assert(a&&b&&c&&!((uintptr_t)a&31u)&&!((uintptr_t)b&31u));
 memset(a,0x12,4096);memset(b,0x34,777);memset(c,0x56,8192);
 uint32_t before=ppc_code_cache_used();ppc_code_cache_trim(a,96);
 assert(ppc_code_cache_used()<before&&((unsigned char *)a)[95]==0x12);
 ppc_code_cache_release(b);void *d=ppc_code_cache_alloc(3500);assert(d);
 for(unsigned i=0;i<8192;i++)assert(((unsigned char *)c)[i]==0x56);
 for(unsigned i=0;i<96;i++)assert(((unsigned char *)a)[i]==0x12);
 ppc_code_cache_release(d);ppc_code_cache_release(a);ppc_code_cache_release(c);
 assert(!ppc_code_cache_used());
 /* Fragment a fully occupied arena and verify live owners stay untouched. */
 void *blocks[512];unsigned n=0;
 while(n<512&&(blocks[n]=ppc_code_cache_alloc(16384))){memset(blocks[n],n&255,16384);n++;}
 assert(n>300&&n<512&&ppc_code_cache_failures());
 for(unsigned i=0;i<n;i+=2)ppc_code_cache_release(blocks[i]);
 assert(!ppc_code_cache_alloc(32768));
 for(unsigned i=1;i<n;i+=2){
  for(unsigned j=0;j<16384;j++)assert(((unsigned char *)blocks[i])[j]==(i&255));
  ppc_code_cache_release(blocks[i]);
 }
 assert(!ppc_code_cache_used());
 a=ppc_code_cache_alloc(PPC_CODE_CACHE_BYTES-32);assert(a);
 assert(!ppc_code_cache_alloc(1));ppc_code_cache_release(a);
 assert(!ppc_code_cache_alloc(0)&&!ppc_code_cache_alloc((size_t)-1));
 gekko2_optimization_mask&=~GEKKO2_OPT_BIT(GEKKO2_OPT_CODE_ARENA);
 b=ppc_code_cache_alloc(256);assert(b&&!ppc_code_cache_owns(b));
 gekko2_optimization_mask|=GEKKO2_OPT_BIT(GEKKO2_OPT_CODE_ARENA);
 ppc_code_cache_release(b);assert(!ppc_code_cache_used());
 puts("PASS stable addresses, trim, fragmented exhaustion, owner isolation, coalescing and heap ownership");
}
