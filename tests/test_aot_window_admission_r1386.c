/* Live source, mapping and event proof for generated arithmetic windows. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/hw/ee_timers.h"
#include "core/hw/ee_intc.h"
#include "core/hw/graphics_scheduler.h"
#include "core/recompiler/optimization.h"
static bios_image_t bios;
static const uint32_t arithmetic[]={0x24420001,0x34630002,0x00042080,0x00a62821};
static ee_state_t *start(uint32_t pc) {
 assert(!system_init(&bios,&bios));
 ee_state_t *s=ee_core_get_state();s->pc=pc;s->next_pc=pc+4;s->cop0[12]=0;s->cop0[9]=100;
 ee_core_display_clock_load(1000);ee_timers_batch_clock[0]=1000;ee_timers_batch_clock[1]=0;
 gekko2_optimization_mask|=GEKKO2_OPT_BIT(GEKKO2_OPT_NATIVE_EVENTS)|GEKKO2_OPT_BIT(GEKKO2_OPT_EE_SERVICES);
 memcpy(s->ram+(pc&0x1fffffff),arithmetic,sizeof arithmetic);return s;
}
static void end(void){ee_core_shutdown();iop_core_shutdown();}
int main(void) {
 for(unsigned bad=0;bad<7;bad++) {
  ee_state_t *s=start(0x80200000);
  if(bad==0)s->ram[0x200008]^=4;
  if(bad==1)s->vu0_running=1;
  if(bad==2)vu1_get_state()->running=1;
  if(bad==3)dma_get_state()->chan[1].chcr=0x100;
  if(bad==4)ee_timers_batch_clock[0]=4;
  if(bad==5){s->cop0[12]=0x18001;s->cop0[11]=102;}
  if(bad==6)s->branch_pending=1;
  ee_state_t before=*s;uint64_t clock=ee_core_display_clock_save();
  assert(!ee_core_native_window_begin_aot(s,s->pc,arithmetic,4,4));
  assert(!memcmp(s,&before,sizeof before));assert(ee_core_display_clock_save()==clock);end();
 }
 ee_state_t *s=start(0x80200ff8);
 assert(ee_core_native_window_begin_aot(s,s->pc,arithmetic,4,4)==2);
 assert(s->pc==0x80201000&&s->instructions_executed==2&&s->cop0[9]==102);end();
 s=start(0x80200000);s->ram[0x20000c]^=4;
 assert(ee_core_native_window_begin_aot(s,s->pc,arithmetic,4,2)==2);end();
 s=start(0x80200000);const uint32_t memory[]={0x8c420000,0x24420001};memcpy(s->ram+0x200000,memory,sizeof memory);
 ee_state_t before=*s;assert(!ee_core_native_window_begin_aot(s,s->pc,memory,2,2));assert(!memcmp(s,&before,sizeof before));end();
 s=start(0x00200000);before=*s;assert(!ee_core_native_window_begin_aot(s,s->pc,arithmetic,4,4));assert(!memcmp(s,&before,sizeof before));end();
 puts("PASS AOT live-word rejection, active VUs/DMA, timer/IRQ/BD deadlines, page boundary, short budgets, memory refusal and unmapped source");return 0;
}
