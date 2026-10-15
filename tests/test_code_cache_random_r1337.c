#include "core/recompiler/ppc_code_cache.h"
#include "core/recompiler/optimization.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
static uint32_t rng=0x12345678;
static unsigned next(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
int main(void)
{
 void *p[512]={0};unsigned size[512]={0};uint8_t value[512]={0};
 gekko2_optimization_mask|=GEKKO2_OPT_CODE_ARENA;
 for(unsigned step=0;step<100000;step++){
  unsigned i=next()%512;
  if(p[i]){
   for(unsigned n=0;n<size[i];n++)assert(((uint8_t*)p[i])[n]==value[i]);
   if(next()&1){ppc_code_cache_release(p[i]);p[i]=0;}
   else{unsigned keep=1+next()%size[i];ppc_code_cache_trim(p[i],keep);size[i]=keep;}
  }else{
   size[i]=1+next()%16384;p[i]=ppc_code_cache_alloc(size[i]);
   if(p[i]){assert(!((uintptr_t)p[i]&31));value[i]=(uint8_t)next();memset(p[i],value[i],size[i]);}
  }
 }
 for(unsigned i=0;i<512;i++)if(p[i]){
  for(unsigned n=0;n<size[i];n++)assert(((uint8_t*)p[i])[n]==value[i]);ppc_code_cache_release(p[i]);
 }
 assert(!ppc_code_cache_used());void *whole=ppc_code_cache_alloc(PPC_CODE_CACHE_BYTES-32);assert(whole);ppc_code_cache_release(whole);
 puts("PASS 100000 randomized arena allocations/releases/trims preserve owner bytes and fully coalesce");
}
