#include "core/runtime_profile.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t clock_value;
uint32_t gekko2_profile_clock(void){return clock_value;}
int main(void)
{
 gekko2_profile p;gekko2_profile_reset();
 assert(gp_enter(GP_GS_RASTER)==GP_COUNT);
 clock_value=0xfffffff0u;gekko2_profile_start(GP_EE);
 gp_work(GP_EE,7);
 clock_value=0x10;unsigned ee=gp_enter(GP_GS_RASTER);
 clock_value=0x20;unsigned gs=gp_enter(GP_GX_WAIT);
 gp_work(GP_GX_WAIT,0);
 clock_value=0x30;gp_leave(gs);
 clock_value=0x40;gp_leave(ee);
 clock_value=0x50;gekko2_profile_enter(GP_IOP);
 clock_value=0x60;gekko2_profile_stop();
 gekko2_profile_get(&p);
 assert(p.ticks[GP_EE]==48&&p.ticks[GP_GS_RASTER]==32&&p.ticks[GP_GX_WAIT]==16&&p.ticks[GP_IOP]==16);
 assert(p.samples==1&&!p.overflows&&!gekko2_profile_active);
 assert(p.entries[GP_EE]==1&&p.entries[GP_GS_RASTER]==1&&p.entries[GP_GX_WAIT]==1&&p.entries[GP_IOP]==1);
 assert(p.work[GP_EE]==7);gp_work(GP_EE,1000);
 clock_value=0x100;gekko2_profile_start_host();
 clock_value=0x110;unsigned front=gp_enter(GP_GX_UPLOAD);assert(front==GP_PRESENT);
 gp_work(GP_GX_UPLOAD,4096);
 clock_value=0x120;gp_leave(front);clock_value=0x130;gekko2_profile_stop();
 gekko2_profile_get_host(&p);assert(p.ticks[GP_PRESENT]==32&&p.ticks[GP_GX_UPLOAD]==16&&p.samples==1);
 assert(p.entries[GP_PRESENT]==1&&p.entries[GP_GX_UPLOAD]==1&&p.work[GP_GX_UPLOAD]==4096&&!p.work[GP_EE]);
 gekko2_profile_get(&p);assert(p.samples==1&&p.ticks[GP_EE]==48&&p.work[GP_EE]==7&&!p.work[GP_GX_UPLOAD]);
 gp_leave(GP_COUNT);gekko2_profile_stop();gekko2_profile_get(&p);assert(p.samples==1);
 gekko2_profile_reset();clock_value=0x200;gekko2_profile_start(GP_EE);
 clock_value=0x210;unsigned root=gp_enter(GP_VU1);gp_work(GP_VU1,3);
 clock_value=0x220;unsigned vu=gp_enter(GP_GS_RASTER);
 clock_value=0x230;gp_leave(vu);clock_value=0x240;gp_leave(root);
 clock_value=0x250;root=gp_enter(GP_EE_NATIVE);gp_work(GP_EE_NATIVE,8);
 clock_value=0x260;gp_leave(root);clock_value=0x270;gekko2_profile_stop();
 gekko2_profile_get(&p);assert(p.ticks[GP_EE]==48&&p.ticks[GP_VU1]==32&&p.ticks[GP_GS_RASTER]==16&&p.ticks[GP_EE_NATIVE]==16);
 assert(p.entries[GP_VU1]==1&&p.work[GP_VU1]==3&&p.work[GP_EE_NATIVE]==8);
 assert(!strcmp(gekko2_profile_name(GP_VU0),"VU0")&&!strcmp(gekko2_profile_unit(GP_GX_UPLOAD),"packed_bytes"));
 assert(!strcmp(gekko2_profile_name(GP_COUNT),"invalid"));
 puts("PASS scope counts/work units and native/VU nested accounting; exclusive nested scopes, inactive no-op, 32-bit TB wrap, sample completion");
}
