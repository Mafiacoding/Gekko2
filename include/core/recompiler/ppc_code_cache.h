#ifndef GEKKO2_PPC_CODE_CACHE_H
#define GEKKO2_PPC_CODE_CACHE_H
#include <stddef.h>
#include <stdint.h>
#ifndef PPC_CODE_CACHE_BYTES
#define PPC_CODE_CACHE_BYTES (6u*1024u*1024u)
#endif
typedef struct {
 uintptr_t begin,end;
 uint32_t used,free_bytes,largest_free_payload,free_spans,live_spans;
 uint64_t allocations,releases,scan_steps,admission_reuses;
} ppc_code_cache_info;
void ppc_code_cache_get_info(ppc_code_cache_info *out);
/* Exact-sized published code has stable addresses. Temporary compilation
 * scratch is relocated before publication; owners release after native returns. */
void *ppc_code_cache_alloc(size_t bytes);
void ppc_code_cache_release(void *code);
/* Trim unused tail in place; address-relative PPC branches must never move. */
void ppc_code_cache_trim(void *code,size_t bytes);
int ppc_code_cache_owns(const void *code);
/* Fault-time owner check: excludes free spans and unpublished heap scratch. */
int ppc_code_cache_live_bounds(const void *pc,uintptr_t *begin,uintptr_t *end);
uint32_t ppc_code_cache_used(void);
/* Non-allocating admission probe; does not increment failure counters. */
int ppc_code_cache_can_alloc(size_t bytes);
/* Changes on successful arena allocation/release, including coalescing. */
uint32_t ppc_code_cache_generation(void);
uint32_t ppc_code_cache_high_water(void);
uint64_t ppc_code_cache_failures(void);
uint32_t ppc_code_cache_capacity(void);
#endif
