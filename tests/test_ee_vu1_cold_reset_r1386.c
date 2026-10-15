#include <assert.h>
#include <stdio.h>
#include "core/ee/ee_core.h"
#include "core/hw/vu.h"
int main(void) {
 bios_image_t bios={0};assert(!ee_core_init(&bios));
 vu1_state_t *v=vu1_get_state();assert(v->vf[0][3]==0x3f800000u);
 v->running=1;v->xgkick_pending=1;v->tpc=0x170;
 v->timing.m_yield=1;v->pipeline.cycle=127;v->instructions_executed=91;
 v->micro[8]=42;v->mem[32]=73;v->vf[1][0]=0x40000000;
 ee_core_shutdown();assert(!ee_core_init(&bios));
 assert(!v->running&&!v->xgkick_pending&&!v->tpc);
 assert(!v->timing.m_yield&&!v->pipeline.cycle&&!v->instructions_executed);
 assert(!v->micro[8]&&!v->mem[32]&&!v->vf[1][0]);
 assert(!v->vf[0][0]&&!v->vf[0][1]&&!v->vf[0][2]&&v->vf[0][3]==0x3f800000u);
 ee_core_shutdown();puts("PASS VU1 cold boot clears previous session execution/timing/data and restores VF00");return 0;
}
