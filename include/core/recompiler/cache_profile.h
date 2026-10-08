#ifndef GEKKO2_CACHE_PROFILE_H
#define GEKKO2_CACHE_PROFILE_H
#include <stdint.h>
#include "core/runtime_profile.h"
typedef struct {
 uint64_t lookups,hits,misses,collisions,stale,attempts,installed,failures;
 uint64_t compile_tb,compile_samples;
} jit_cache_profile;
/* Sample one in 64 attempts; avoid two clock reads on every translation.
 * The returned high bit marks a live sample, independently of TB wrapping. */
static inline uint64_t jit_compile_begin(jit_cache_profile *p)
{
 p->attempts++;
 return (p->attempts&63u)==1u ? (1ull<<32)|gekko2_profile_clock() : 0;
}
static inline void jit_compile_end(jit_cache_profile *p,uint64_t sample)
{
 if(sample){p->compile_tb+=(uint32_t)(gekko2_profile_clock()-(uint32_t)sample);p->compile_samples++;}
}
#endif
