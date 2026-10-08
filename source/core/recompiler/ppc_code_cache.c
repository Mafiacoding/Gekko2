#include "core/recompiler/ppc_code_cache.h"
#include "core/recompiler/optimization.h"
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <limits.h>
#define HEADER 32u
#define LIVE 0x47434f44u
typedef struct {uint32_t span,next,magic,pad[5];} chunk;
#if !defined(GEKKO2_CODE_ARENA_DISABLE)
static uint8_t arena[PPC_CODE_CACHE_BYTES] __attribute__((aligned(32)));
/* Free-list links are offsets + 1; zero is the terminator. */
static uint32_t first,used,high_water,cache_initialized;
static uint64_t failures;
static chunk *node(uint32_t link){return (chunk *)(void *)(arena+link-1u);}
static void init(void)
{
 if(cache_initialized)return;
 chunk *p=(chunk *)(void *)arena;memset(p,0,HEADER);
 p->span=PPC_CODE_CACHE_BYTES;first=1u;cache_initialized=1u;
}
int ppc_code_cache_owns(const void *code)
{
 uintptr_t p=(uintptr_t)code,base=(uintptr_t)arena;
 return p>=base+HEADER&&p<base+PPC_CODE_CACHE_BYTES;
}
void *ppc_code_cache_alloc(size_t bytes)
{
 if(!gekko2_opt_enabled(GEKKO2_OPT_CODE_ARENA))return bytes?memalign(32,bytes):NULL;
 init();
 if(!bytes||bytes>PPC_CODE_CACHE_BYTES-HEADER){failures++;return NULL;}
 uint32_t span=((uint32_t)bytes+31u)&~31u;span+=HEADER;
 uint32_t *edge=&first;
 while(*edge) {
  chunk *p=node(*edge);
  if(p->span>=span) {
   uint32_t offset=*edge-1u,left=p->span-span,next=p->next;
   if(left>=HEADER+32u) {
    chunk *tail=(chunk *)(void *)(arena+offset+span);
    memset(tail,0,HEADER);tail->span=left;tail->next=next;
    *edge=offset+span+1u;p->span=span;
   } else *edge=next;
   p->next=0;p->magic=LIVE;used+=p->span;
   if(used>high_water)high_water=used;
   return (uint8_t *)p+HEADER;
  }
  edge=&p->next;
 }
 failures++;return NULL;
}
void ppc_code_cache_release(void *code)
{
 if(!code)return;
 if(!ppc_code_cache_owns(code)){free(code);return;}
 uintptr_t offset=(uintptr_t)code-(uintptr_t)arena-HEADER;
 if((offset&31u)||offset>PPC_CODE_CACHE_BYTES-HEADER)return;
 chunk *p=(chunk *)(void *)(arena+offset);
 if(p->magic!=LIVE||p->span<HEADER||p->span>PPC_CODE_CACHE_BYTES-offset)return;
 used-=p->span;p->magic=0;
 uint32_t link=(uint32_t)offset+1u,*edge=&first,previous=0;
 while(*edge&&*edge<link){previous=*edge;edge=&node(*edge)->next;}
 p->next=*edge;*edge=link;
 if(p->next&&offset+p->span==p->next-1u) {
  chunk *next=node(p->next);p->span+=next->span;p->next=next->next;
 }
 if(previous) {
  chunk *prev=node(previous);
  if(previous-1u+prev->span==offset){prev->span+=p->span;prev->next=p->next;}
 }
}
uint32_t ppc_code_cache_used(void){return used;}
void ppc_code_cache_trim(void *code,size_t bytes)
{
 if(!code||!bytes||!ppc_code_cache_owns(code)||bytes>PPC_CODE_CACHE_BYTES-HEADER)return;
 uintptr_t offset=(uintptr_t)code-(uintptr_t)arena-HEADER;
 if(offset&31u)return;
 chunk *p=(chunk *)(void *)(arena+offset);
 uint32_t span=(((uint32_t)bytes+31u)&~31u)+HEADER;
 if(p->magic!=LIVE||span>p->span||p->span-span<HEADER+32u)return;
 uint32_t left=p->span-span;p->span=span;
 chunk *tail=(chunk *)(void *)(arena+offset+span);
 memset(tail,0,HEADER);tail->span=left;tail->magic=LIVE;
 ppc_code_cache_release((uint8_t *)tail+HEADER);
}
uint32_t ppc_code_cache_high_water(void){return high_water;}
uint64_t ppc_code_cache_failures(void){return failures;}
uint32_t ppc_code_cache_capacity(void){return PPC_CODE_CACHE_BYTES;}
#else
int ppc_code_cache_owns(const void *p){(void)p;return 0;}
void *ppc_code_cache_alloc(size_t bytes){return memalign(32,bytes);}
void ppc_code_cache_release(void *p){free(p);}
void ppc_code_cache_trim(void *p,size_t bytes){(void)p;(void)bytes;}
uint32_t ppc_code_cache_used(void){return 0;}
uint32_t ppc_code_cache_high_water(void){return 0;}
uint64_t ppc_code_cache_failures(void){return 0;}
uint32_t ppc_code_cache_capacity(void){return 0;}
#endif
