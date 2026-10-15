#include "core/runtime_profile.h"
#include <assert.h>
#include <stdio.h>
static uint32_t clock_value;
uint32_t gekko2_profile_clock(void){return clock_value;}
int main(void)
{
 gekko2_profile p;gekko2_profile_reset();
 assert(gp_enter(GP_GS_RASTER)==GP_COUNT);
 clock_value=0xfffffff0u;gekko2_profile_start(GP_EE);
 clock_value=0x10;unsigned ee=gp_enter(GP_GS_RASTER);
 clock_value=0x20;unsigned gs=gp_enter(GP_GX_WAIT);
 clock_value=0x30;gp_leave(gs);
 clock_value=0x40;gp_leave(ee);
 clock_value=0x50;gekko2_profile_enter(GP_IOP);
 clock_value=0x60;gekko2_profile_stop();
 gekko2_profile_get(&p);
 assert(p.ticks[GP_EE]==48&&p.ticks[GP_GS_RASTER]==32&&p.ticks[GP_GX_WAIT]==16&&p.ticks[GP_IOP]==16);
 assert(p.samples==1&&!p.overflows&&!gekko2_profile_active);
 clock_value=0x100;gekko2_profile_start_host();
 clock_value=0x110;unsigned front=gp_enter(GP_GX_UPLOAD);assert(front==GP_PRESENT);
 clock_value=0x120;gp_leave(front);clock_value=0x130;gekko2_profile_stop();
 gekko2_profile_get_host(&p);assert(p.ticks[GP_PRESENT]==32&&p.ticks[GP_GX_UPLOAD]==16&&p.samples==1);
 gekko2_profile_get(&p);assert(p.samples==1&&p.ticks[GP_EE]==48);
 gp_leave(GP_COUNT);gekko2_profile_stop();gekko2_profile_get(&p);assert(p.samples==1);
 puts("PASS exclusive nested scopes, inactive no-op, 32-bit TB wrap, sample completion");
}
