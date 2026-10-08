#ifndef GEKKO2_PPC_CODE_CACHE_H
#define GEKKO2_PPC_CODE_CACHE_H
#include <stddef.h>
#include <stdint.h>
#define PPC_CODE_CACHE_BYTES (6u*1024u*1024u)
/* Exact-sized published code has stable addresses. Temporary compilation
 * scratch is relocated before publication; owners release after native returns. */
void *ppc_code_cache_alloc(size_t bytes);
void ppc_code_cache_release(void *code);
/* Trim unused tail in place; address-relative PPC branches must never move. */
void ppc_code_cache_trim(void *code,size_t bytes);
int ppc_code_cache_owns(const void *code);
uint32_t ppc_code_cache_used(void);
uint32_t ppc_code_cache_high_water(void);
uint64_t ppc_code_cache_failures(void);
uint32_t ppc_code_cache_capacity(void);
#endif
