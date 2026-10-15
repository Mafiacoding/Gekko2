#include "core/recompiler/ppc_code_cache.h"
#include "core/recompiler/optimization.h"
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <limits.h>
#define HEADER 32u
#define LIVE 0x47434f44u
#define BINS 96u
typedef struct {uint32_t span,next,magic,previous_span,free_previous,pad[3];} chunk;
#if !defined(GEKKO2_CODE_ARENA_DISABLE)
static uint8_t arena[PPC_CODE_CACHE_BYTES] __attribute__((aligned(32)));
/* Physical boundary tags permit constant-time neighbour coalescing. Size
 * classes skip irrelevant holes; every published owner remains stationary. */
static uint32_t heads[BINS],bitmap[BINS/32u],used,high_water,cache_initialized,generation;
static uint64_t failures,allocations,releases,scan_steps,admission_reuses;
static uint32_t probe_span,probe_link,probe_generation;
static unsigned probe_valid;
static chunk *node(uint32_t link){return (chunk *)(void *)(arena+link-1u);}
static uint32_t link_for(chunk *p){return (uint32_t)((uint8_t*)p-arena)+1u;}
/* Exact aligned classes for small blocks, four classes per power of two
 * above 1 KiB. Three 32-bit bitmap words keep lookup native on Broadway. */
static unsigned bin_for(uint32_t span)
{
 if(span<=1024u)return span/32u-1u;
 unsigned exponent=31u-(unsigned)__builtin_clz(span);
 return 32u+(exponent-10u)*4u+((span>>(exponent-2u))&3u);
}
static void list_add(chunk *p)
{
 unsigned b=bin_for(p->span);uint32_t link=link_for(p);
 p->magic=0;p->free_previous=0;p->next=heads[b];
 if(p->next)node(p->next)->free_previous=link;
 heads[b]=link;bitmap[b>>5]|=1u<<(b&31u);
}
static void list_remove(chunk *p)
{
 unsigned b=bin_for(p->span);
 if(p->free_previous)node(p->free_previous)->next=p->next;else heads[b]=p->next;
 if(p->next)node(p->next)->free_previous=p->free_previous;
 if(!heads[b])bitmap[b>>5]&=~(1u<<(b&31u));
 p->next=p->free_previous=0;
}
static void set_next_previous(chunk *p)
{
 uint32_t offset=link_for(p)-1u+p->span;
 if(offset<PPC_CODE_CACHE_BYTES)((chunk *)(void *)(arena+offset))->previous_span=p->span;
}
static void init(void)
{
 if(cache_initialized)return;
 chunk *p=(chunk *)(void *)arena;memset(p,0,HEADER);p->span=PPC_CODE_CACHE_BYTES;
 list_add(p);cache_initialized=1u;
}
int ppc_code_cache_owns(const void *code)
{
 uintptr_t p=(uintptr_t)code,base=(uintptr_t)arena;
 return p>=base+HEADER&&p<base+PPC_CODE_CACHE_BYTES;
}
uint32_t ppc_code_cache_generation(void){return generation;}
int ppc_code_cache_live_bounds(const void *pc,uintptr_t *begin,uintptr_t *end)
{
 uintptr_t address=(uintptr_t)pc;
 if(!cache_initialized||!ppc_code_cache_owns(pc))return 0;
 for(uint32_t offset=0;offset<PPC_CODE_CACHE_BYTES;){
  chunk *p=(chunk *)(void *)(arena+offset);uint32_t span=p->span;
  if(span<HEADER+32u||(span&31u)||span>PPC_CODE_CACHE_BYTES-offset)return 0;
  uintptr_t lo=(uintptr_t)p+HEADER,hi=(uintptr_t)p+span;
  if(address>=lo&&address<hi){
   if(p->magic!=LIVE)return 0;
   *begin=lo;*end=hi;return 1;
  }
  offset+=span;
 }
 return 0;
}
/* Reuse the admission result only while no allocation/release has changed
 * the arena. EE make-room probes the exact final size immediately before
 * finalize; repeating the hole walk there wasted work on every new block. */
static chunk *find_free(uint32_t span)
{
 if(probe_valid&&probe_span==span&&probe_generation==generation){
  admission_reuses++;return probe_link?node(probe_link):NULL;
 }
 probe_valid=1;probe_span=span;probe_generation=generation;probe_link=0;
 unsigned first=bin_for(span);
 for(unsigned word=first>>5;word<BINS/32u;word++){
  uint32_t candidates=bitmap[word];
  if(word==(first>>5))candidates&=~0u<<(first&31u);
  while(candidates){
   unsigned bit=(unsigned)__builtin_ctz(candidates),b=word*32u+bit;
   for(uint32_t link=heads[b];link;link=node(link)->next){
    scan_steps++;
    if(node(link)->span>=span){probe_link=link;return node(link);}
   }
   candidates&=~(1u<<bit);
  }
 }
 return NULL;
}
int ppc_code_cache_can_alloc(size_t bytes)
{
 if(!gekko2_opt_enabled(GEKKO2_OPT_CODE_ARENA))return 1;
 init();if(!bytes||bytes>PPC_CODE_CACHE_BYTES-HEADER)return 0;
 uint32_t span=(((uint32_t)bytes+31u)&~31u)+HEADER;
 return find_free(span)!=NULL;
}
static void changed(void){probe_valid=0;if(++generation==0u)generation=1u;}
void *ppc_code_cache_alloc(size_t bytes)
{
 if(!gekko2_opt_enabled(GEKKO2_OPT_CODE_ARENA))return bytes?memalign(32,bytes):NULL;
 init();if(!bytes||bytes>PPC_CODE_CACHE_BYTES-HEADER){failures++;return NULL;}
 uint32_t span=(((uint32_t)bytes+31u)&~31u)+HEADER;
 chunk *p=find_free(span);
 if(p){
  list_remove(p);uint32_t left=p->span-span;
  if(left>=HEADER+32u){
   chunk *tail=(chunk *)(void *)((uint8_t*)p+span);memset(tail,0,HEADER);
   tail->span=left;tail->previous_span=span;p->span=span;
   set_next_previous(tail);list_add(tail);
  }
  p->magic=LIVE;used+=p->span;allocations++;changed();if(used>high_water)high_water=used;
  return (uint8_t*)p+HEADER;
 }
 failures++;return NULL;
}
void ppc_code_cache_release(void *code)
{
 if(!code)return;if(!ppc_code_cache_owns(code)){free(code);return;}
 uintptr_t offset=(uintptr_t)code-(uintptr_t)arena-HEADER;
 if((offset&31u)||offset>PPC_CODE_CACHE_BYTES-HEADER)return;
 chunk *p=(chunk *)(void *)(arena+offset);
 if(p->magic!=LIVE||p->span<HEADER+32u||(p->span&31u)||p->span>PPC_CODE_CACHE_BYTES-offset)return;
 used-=p->span;p->magic=0;releases++;changed();
 if(p->previous_span&&p->previous_span<=offset){
  chunk *previous=(chunk *)(void *)((uint8_t*)p-p->previous_span);
  if(!previous->magic){list_remove(previous);previous->span+=p->span;p=previous;}
 }
 uint32_t next=link_for(p)-1u+p->span;
 if(next<PPC_CODE_CACHE_BYTES){chunk *n=(chunk *)(void *)(arena+next);
  if(!n->magic){list_remove(n);p->span+=n->span;}
 }
 set_next_previous(p);list_add(p);
}
uint32_t ppc_code_cache_used(void){return used;}
void ppc_code_cache_trim(void *code,size_t bytes)
{
 if(!code||!bytes||!ppc_code_cache_owns(code)||bytes>PPC_CODE_CACHE_BYTES-HEADER)return;
 uintptr_t offset=(uintptr_t)code-(uintptr_t)arena-HEADER;if(offset&31u)return;
 chunk *p=(chunk *)(void *)(arena+offset);uint32_t span=(((uint32_t)bytes+31u)&~31u)+HEADER;
 if(p->magic!=LIVE||span>p->span||p->span-span<HEADER+32u)return;
 uint32_t left=p->span-span;p->span=span;
 chunk *tail=(chunk *)(void *)(arena+offset+span);memset(tail,0,HEADER);
 tail->span=left;tail->previous_span=span;tail->magic=LIVE;set_next_previous(tail);
 ppc_code_cache_release((uint8_t*)tail+HEADER);releases--; /* tail was not a published owner */
}
uint32_t ppc_code_cache_high_water(void){return high_water;}
uint64_t ppc_code_cache_failures(void){return failures;}
uint32_t ppc_code_cache_capacity(void){return PPC_CODE_CACHE_BYTES;}
void ppc_code_cache_get_info(ppc_code_cache_info *out)
{
 if(!out)return;memset(out,0,sizeof(*out));init();
 out->begin=(uintptr_t)arena;out->end=out->begin+PPC_CODE_CACHE_BYTES;
 out->used=used;out->free_bytes=PPC_CODE_CACHE_BYTES-used;
 out->allocations=allocations;out->releases=releases;
 out->scan_steps=scan_steps;out->admission_reuses=admission_reuses;
 for(uint32_t offset=0;offset<PPC_CODE_CACHE_BYTES;){
  chunk *p=(chunk *)(void *)(arena+offset);
  if(p->magic==LIVE)out->live_spans++;
  else {out->free_spans++;if(p->span-HEADER>out->largest_free_payload)out->largest_free_payload=p->span-HEADER;}
  offset+=p->span;
 }
}

#else
void ppc_code_cache_get_info(ppc_code_cache_info *out){if(out)memset(out,0,sizeof(*out));}
int ppc_code_cache_can_alloc(size_t bytes){return bytes!=0;}
uint32_t ppc_code_cache_generation(void){return 0;}
int ppc_code_cache_owns(const void *p){(void)p;return 0;}
int ppc_code_cache_live_bounds(const void *p,uintptr_t *b,uintptr_t *e){(void)p;(void)b;(void)e;return 0;}
void *ppc_code_cache_alloc(size_t bytes){return memalign(32,bytes);}
void ppc_code_cache_release(void *p){free(p);}
void ppc_code_cache_trim(void *p,size_t bytes){(void)p;(void)bytes;}
uint32_t ppc_code_cache_used(void){return 0;}
uint32_t ppc_code_cache_high_water(void){return 0;}
uint64_t ppc_code_cache_failures(void){return 0;}
uint32_t ppc_code_cache_capacity(void){return 0;}
#endif
