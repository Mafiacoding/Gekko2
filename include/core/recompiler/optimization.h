#ifndef GEKKO2_OPTIMIZATION_H
#define GEKKO2_OPTIMIZATION_H
#include <stdint.h>
enum {
 GEKKO2_OPT_EE_JIT, GEKKO2_OPT_IOP_JIT, GEKKO2_OPT_VU_JIT,
 GEKKO2_OPT_EE_BLOCKS, GEKKO2_OPT_NATIVE_LINKS, GEKKO2_OPT_RESIDENCY,
 GEKKO2_OPT_CODE_ARENA, GEKKO2_OPT_GX_RESIDENT, GEKKO2_OPT_GX_SOURCE_CACHE,
 GEKKO2_OPT_FASTMEM, GEKKO2_OPT_GX_DISJOINT_WRITES,
 GEKKO2_OPT_WORD_ALLOCATION, GEKKO2_OPT_GX_GOURAUD, GEKKO2_OPT_CACHE_REUSE, GEKKO2_OPT_COUNT
};
#define GEKKO2_OPT_BIT(n) (1u<<(n))
#define GEKKO2_OPT_BASE_DEFAULT ((0x1ffu | GEKKO2_OPT_BIT(GEKKO2_OPT_GX_DISJOINT_WRITES) | GEKKO2_OPT_BIT(GEKKO2_OPT_WORD_ALLOCATION) | GEKKO2_OPT_BIT(GEKKO2_OPT_GX_GOURAUD)) & ~GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS))
#if defined(GEKKO2_EE_BLOCK_FAST) || defined(PCSX2WII_FAST)
#define GEKKO2_OPT_BLOCK_DEFAULT GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS)
#else
#define GEKKO2_OPT_BLOCK_DEFAULT 0u
#endif
#ifdef GEKKO2_FASTMEM_DEFAULT
#define GEKKO2_OPT_FASTMEM_DEFAULT GEKKO2_OPT_BIT(GEKKO2_OPT_FASTMEM)
#else
#define GEKKO2_OPT_FASTMEM_DEFAULT 0u
#endif
#ifdef GEKKO2_CACHE_LEGACY_DEFAULT
#define GEKKO2_OPT_CACHE_DEFAULT 0u
#else
#define GEKKO2_OPT_CACHE_DEFAULT GEKKO2_OPT_BIT(GEKKO2_OPT_CACHE_REUSE)
#endif
#define GEKKO2_OPT_DEFAULT (GEKKO2_OPT_CACHE_DEFAULT | GEKKO2_OPT_BASE_DEFAULT | GEKKO2_OPT_BLOCK_DEFAULT | GEKKO2_OPT_FASTMEM_DEFAULT)
/* Optional for stand-alone emitter tests; production links optimization.c. */
#ifdef GEKKO
extern uint32_t gekko2_optimization_mask;
#else
extern uint32_t gekko2_optimization_mask __attribute__((weak));
#endif
static inline int gekko2_opt_enabled(unsigned n)
{
#ifdef GEKKO
 uint32_t mask=gekko2_optimization_mask;
#else
 uint32_t mask=&gekko2_optimization_mask?gekko2_optimization_mask:GEKKO2_OPT_DEFAULT;
#endif
 return n<GEKKO2_OPT_COUNT && !!(mask&GEKKO2_OPT_BIT(n));
}
uint32_t gekko2_opt_available(void);
uint32_t gekko2_opt_requested(void);
const char *gekko2_opt_name(unsigned n);
const char *gekko2_opt_description(unsigned n);
int gekko2_opt_toggle(unsigned n);
/* Apply ONLY before a cold system_init; never to executing native caches. */
void gekko2_opt_apply(void);
int gekko2_opt_load(const char *path);
int gekko2_opt_save(const char *path);
#endif
