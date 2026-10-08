#ifndef GEKKO2_RUNTIME_PROFILE_H
#define GEKKO2_RUNTIME_PROFILE_H
#include <stdint.h>
/* Exclusive TB samples within the existing randomized EE/IOP sample. These
 * are sampled host costs, never guest cycles or an estimate of FPS. */
enum { GP_EE, GP_IOP, GP_SCHEDULER, GP_IDLE, GP_COMPILE,
       GP_GS_RASTER, GP_GX_UPLOAD, GP_GX_READBACK, GP_GX_WAIT, GP_PRESENT, GP_COUNT };
typedef struct { uint64_t ticks[GP_COUNT], samples, overflows; } gekko2_profile;
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
void gekko2_profile_get(gekko2_profile *out) GP_OPTIONAL;
static inline unsigned gp_enter(unsigned category)
{
#ifndef GEKKO
 if(!&gekko2_profile_active)return GP_COUNT;
#endif
 return gekko2_profile_active?gekko2_profile_enter(category):GP_COUNT;
}
static inline void gp_leave(unsigned previous)
{ if(previous<GP_COUNT)gekko2_profile_leave(previous); }
#endif
