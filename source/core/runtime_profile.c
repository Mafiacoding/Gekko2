#include "core/runtime_profile.h"
#include <string.h>
unsigned gekko2_profile_active;
static unsigned current;
static uint32_t previous_clock;
static gekko2_profile totals,host_totals;
static unsigned host_mode;
__attribute__((weak,noinline)) uint32_t gekko2_profile_clock(void)
{
#ifdef __PPC__
 uint32_t value;__asm__ volatile("mftb %0":"=r"(value));return value;
#else
 return 0;
#endif
}
static void account(void)
{
 uint32_t now=gekko2_profile_clock();
 (host_mode?host_totals.ticks:totals.ticks)[current]+=(uint32_t)(now-previous_clock);previous_clock=now;
}
void gekko2_profile_start(unsigned category)
{
 if(gekko2_profile_active||category>=GP_COUNT){totals.overflows++;return;}
 host_mode=0;current=category;previous_clock=gekko2_profile_clock();gekko2_profile_active=1;
}
void gekko2_profile_stop(void)
{
 if(!gekko2_profile_active)return;
 account();gekko2_profile_active=0;if(host_mode)host_totals.samples++;else totals.samples++;
}
unsigned gekko2_profile_enter(unsigned category)
{
 if(!gekko2_profile_active||category>=GP_COUNT)return GP_COUNT;
 account();unsigned old=current;current=category;return old;
}
void gekko2_profile_leave(unsigned previous)
{ if(gekko2_profile_active&&previous<GP_COUNT){account();current=previous;} }
void gekko2_profile_reset(void)
{ memset(&totals,0,sizeof totals);memset(&host_totals,0,sizeof host_totals);host_mode=0;gekko2_profile_active=0; }
void gekko2_profile_get(gekko2_profile *out){if(out)*out=totals;}

void gekko2_profile_start_host(void)
{
 if(gekko2_profile_active){host_totals.overflows++;return;}
 host_mode=1;current=GP_PRESENT;previous_clock=gekko2_profile_clock();gekko2_profile_active=1;
}
void gekko2_profile_get_host(gekko2_profile *out){if(out)*out=host_totals;}
