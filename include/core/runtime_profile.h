#ifndef GEKKO2_RUNTIME_PROFILE_H
#define GEKKO2_RUNTIME_PROFILE_H
#include <stdint.h>
/* entries counts scope starts/enters (including declined native attempts).
 * work has category-specific units, not additive across categories.
 * Exclusive TB samples within the existing randomized EE/IOP sample. These
 * are sampled host costs, never guest cycles or an estimate of FPS. */
enum { GP_EE, GP_IOP, GP_SCHEDULER, GP_IDLE, GP_COMPILE,
       GP_GS_RASTER, GP_GX_UPLOAD, GP_GX_READBACK, GP_GX_WAIT, GP_PRESENT, GP_IPU, GP_EE_NATIVE, GP_VU0, GP_VU1, GP_COUNT };
typedef struct { uint64_t ticks[GP_COUNT], samples, overflows;
                 uint64_t entries[GP_COUNT], work[GP_COUNT]; } gekko2_profile;
#ifdef GEKKO
#define GP_OPTIONAL
#else
#define GP_OPTIONAL __attribute__((weak))
#endif
extern unsigned gekko2_profile_active GP_OPTIONAL;
uint32_t gekko2_profile_clock(void);
void gekko2_profile_start(unsigned category) GP_OPTIONAL;
void gekko2_profile_stop(void) GP_OPTIONAL;
void gekko2_profile_start_host(void);
void gekko2_profile_get_host(gekko2_profile *out);
unsigned gekko2_profile_enter(unsigned category) GP_OPTIONAL;
void gekko2_profile_leave(unsigned previous) GP_OPTIONAL;
void gekko2_profile_reset(void) GP_OPTIONAL;
void gekko2_profile_work(unsigned category,uint64_t amount) GP_OPTIONAL;
const char *gekko2_profile_name(unsigned category);
const char *gekko2_profile_unit(unsigned category);
void gekko2_profile_get(gekko2_profile *out) GP_OPTIONAL;
static inline unsigned gp_enter(unsigned category)
{
#ifndef GEKKO
 if(!&gekko2_profile_active)return GP_COUNT;
#endif
 return gekko2_profile_active?gekko2_profile_enter(category):GP_COUNT;
}
static inline void gp_work(unsigned category,uint64_t amount)
{
#ifndef GEKKO
 if(!&gekko2_profile_active)return;
#endif
 if(gekko2_profile_active)gekko2_profile_work(category,amount);
}
static inline void gp_leave(unsigned previous)
{ if(previous<GP_COUNT)gekko2_profile_leave(previous); }
#endif
