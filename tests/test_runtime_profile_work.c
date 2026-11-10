#include "core/runtime_profile.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/hw/vu.h"
#include "core/hw/gs_gx.h"
#include "core/recompiler/optimization.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t clock_value;
uint32_t gekko2_profile_clock(void){return clock_value+=10;}
static void put(uint8_t *p,uint32_t w){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(w>>(8*i));}
int main(void)
{
 bios_image_t b={0};assert(!system_init(&b,&b));gekko2_optimization_mask=0;
 ee_state_t *ee=ee_core_get_state();ee->pc=0x80200000;ee->next_pc=ee->pc+4;ee->cop0[12]=0;
 for(unsigned i=0;i<4;i++)put(ee->ram+0x200000+i*4,0x24010007);
 gekko2_profile_reset();gekko2_profile_start(GP_EE);assert(ee_core_step_n(4)==4);gekko2_profile_stop();
 gekko2_profile p;gekko2_profile_get(&p);assert(p.work[GP_EE]==4&&ee->gpr[1].ud0==7);
 vu1_init();vu1_state_t *v=vu1_get_state();
 put(v->micro,0x800003bf);put(v->micro+4,0x400002ff);put(v->micro+8,0x800003bf);put(v->micro+12,0x2ff);
 uint64_t before=v->instructions_executed;
 gekko2_profile_start(GP_EE);vu1_exec_micro(0);gekko2_profile_stop();gekko2_profile_get(&p);
 assert(!v->running&&p.entries[GP_VU1]==1&&p.work[GP_VU1]==v->instructions_executed-before&&p.work[GP_VU1]>0);
 uint8_t packed[64];gs_gx_texture_draw d={0};d.tw=d.th=4;
 gekko2_profile_start_host();assert(gs_gx_pack_rgba8(packed,sizeof packed,0,64,0,0,4,4)==64);gekko2_profile_stop();
 gekko2_profile_get_host(&p);assert(p.entries[GP_GX_UPLOAD]==1&&p.work[GP_GX_UPLOAD]==64);
 puts("PASS real EE retirement, VU1 launch and GX packing instrumentation; state and separate work units");return 0;
}
