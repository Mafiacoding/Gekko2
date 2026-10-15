#include <assert.h>
#include <stdio.h>
#include "core/recompiler/optimization.h"
uint32_t gekko2_optimization_mask=GEKKO2_OPT_BIT(GEKKO2_OPT_CODE_ARENA);
#include "core/recompiler/ppc_code_cache.c"
static unsigned rng=0x31415926u;
static unsigned next(void){rng=rng*1664525u+1013904223u;return rng;}
int main(void){
 void *p[128]={0};unsigned n[128]={0};
 for(unsigned t=0;t<12000;t++){
  unsigned i=next()%128;
  if(p[i]){
   assert(((unsigned char*)p[i])[0]==(unsigned char)i);
   assert(((unsigned char*)p[i])[n[i]-1]==(unsigned char)(i^0x55));
   uint32_t epoch=ppc_code_cache_generation();ppc_code_cache_release(p[i]);p[i]=0;
   assert(ppc_code_cache_generation()!=epoch);
  }else{
   unsigned size=(next()%65535u)+2u;
   uint32_t used=ppc_code_cache_used(),epoch=ppc_code_cache_generation();uint64_t failures=ppc_code_cache_failures();
   int fit=ppc_code_cache_can_alloc(size);
   assert(ppc_code_cache_used()==used&&ppc_code_cache_generation()==epoch&&ppc_code_cache_failures()==failures);
   p[i]=ppc_code_cache_alloc(size);n[i]=size;assert(!!p[i]==fit);
   if(p[i]){((unsigned char*)p[i])[0]=(unsigned char)i;((unsigned char*)p[i])[size-1]=(unsigned char)(i^0x55);}
  }
 }
 for(unsigned i=0;i<128;i++)if(p[i])ppc_code_cache_release(p[i]);
 assert(ppc_code_cache_used()==0&&ppc_code_cache_can_alloc(PPC_CODE_CACHE_BYTES-32u));
 assert(!ppc_code_cache_can_alloc(0)&&!ppc_code_cache_can_alloc(SIZE_MAX));
 puts("PASS 12000 fragmented admission/allocation/release operations; epoch, bytes and full coalescing");
}
