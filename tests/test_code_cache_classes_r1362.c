#include "core/recompiler/ppc_code_cache.h"
#include "core/recompiler/optimization.h"
#include <stdio.h>
#include <assert.h>
int main(void){
 void *holes[1000],*guards[1000];
 gekko2_optimization_mask|=GEKKO2_OPT_BIT(GEKKO2_OPT_CODE_ARENA);
 for(unsigned i=0;i<1000;i++){
  holes[i]=ppc_code_cache_alloc(96);guards[i]=ppc_code_cache_alloc(32);
  assert(holes[i]&&guards[i]);
 }
 for(unsigned i=0;i<1000;i++)ppc_code_cache_release(holes[i]);
 ppc_code_cache_info before,after;ppc_code_cache_get_info(&before);
 void *p=ppc_code_cache_alloc(160);assert(p);ppc_code_cache_get_info(&after);
 unsigned long long scans=after.scan_steps-before.scan_steps;
 printf("Too-small holes=1000, allocation scans=%llu\n",scans);
 ppc_code_cache_release(p);
 for(unsigned i=0;i<1000;i++)ppc_code_cache_release(guards[i]);
 assert(ppc_code_cache_used()==0);void *all=ppc_code_cache_alloc(PPC_CODE_CACHE_BYTES-32);assert(all);ppc_code_cache_release(all);
 if(scans>2)return 1;
 puts("PASS size-class lookup skips unsuitable holes; full coalescing restored");return 0;
}
