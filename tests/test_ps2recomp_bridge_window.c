/* Bridge-level live scheduler admission checks. No PPC code is executed here. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/iop/iop_core.h"
#include "libchdr/chd.h"
#include "core/hw/ee_timers.h"
#include "core/hw/ee_intc.h"
#include "core/hw/graphics_scheduler.h"
#include "core/hw/dma.h"
#include "core/recompiler/optimization.h"
#include "core/recompiler/ps2recomp_bridge.h"

static bios_image_t bios;

static const uint32_t arithmetic[]={0x24420001,0x34630002,0x00042080,0x00a62821};

/* The focused host runner links chd_reader.c but not libchdr itself.
 * This fixture never opens a disc; provide the one unused symbol required
 * by system_init's retained host link graph. */
chd_error chd_read(chd_file *chd,uint32_t hunknum,void *buffer)
{(void)chd;(void)hunknum;(void)buffer;return CHDERR_INVALID_PARAMETER;}

static ee_state_t *start(uint32_t pc,ee_recomp_bridge_t *bridge)
{
 assert(!system_init(&bios,&bios));
 ee_state_t *s=ee_core_get_state();
 s->pc=pc;s->next_pc=pc+4;s->cop0[12]=0;s->cop0[9]=100;
 ee_core_display_clock_load(1000);
 ee_timers_batch_clock[0]=1000;ee_timers_batch_clock[1]=0;
 gekko2_optimization_mask|=GEKKO2_OPT_BIT(GEKKO2_OPT_NATIVE_EVENTS)|
                             GEKKO2_OPT_BIT(GEKKO2_OPT_EE_SERVICES);
 memcpy(s->ram+(pc&0x1fffffffu),arithmetic,sizeof arithmetic);
 assert(ee_recomp_bridge_bind(bridge,s)==EE_RECOMP_BRIDGE_OK);
 return s;
}
static void finish(void){ee_core_shutdown();iop_core_shutdown();}

int main(void)
{
 for(unsigned bad=0;bad<7;bad++) {
  ee_recomp_bridge_t bridge={0};
  ee_state_t *s=start(0x80200000u,&bridge);
  if(bad==0)s->ram[0x200008]^=4;
  if(bad==1)s->vu0_running=1;
  if(bad==2)vu1_get_state()->running=1;
  if(bad==3)dma_get_state()->chan[1].chcr=0x100;
  if(bad==4)ee_timers_batch_clock[0]=4;
  if(bad==5){s->cop0[12]=0x18001;s->cop0[11]=102;}
  if(bad==6)s->branch_pending=1;
  ee_state_t before=*s;uint64_t clock=ee_core_display_clock_save();
  assert(ee_recomp_bridge_begin_native_window(&bridge,s->pc,arithmetic,4,4,0)==0);
  assert(!memcmp(s,&before,sizeof before));
  assert(ee_core_display_clock_save()==clock);
  finish();
 }
 {
  ee_recomp_bridge_t bridge={0};
  ee_state_t *s=start(0x80200ff8u,&bridge);
  assert(ee_recomp_bridge_begin_native_window(&bridge,s->pc,arithmetic,4,4,0)==2);
  assert(s->pc==0x80201000u&&s->instructions_executed==2&&s->cop0[9]==102);
  finish();
 }
 {
  ee_recomp_bridge_t bridge={0};
  ee_state_t *s=start(0x80200000u,&bridge);
  s->ram[0x20000cu]^=4;
  assert(ee_recomp_bridge_begin_native_window(&bridge,s->pc,arithmetic,4,2,0)==2);
  finish();
 }
 {
  ee_recomp_bridge_t bridge={0};
  ee_state_t *s=start(0x80200000u,&bridge);
  const uint32_t memory[]={0x8c420000,0x24420001};
  memcpy(s->ram+0x200000,memory,sizeof memory);
  ee_state_t before=*s;
  assert(ee_recomp_bridge_begin_native_window(&bridge,s->pc,memory,2,2,0)==0);
  assert(!memcmp(s,&before,sizeof before));
  finish();
 }
 {
  ee_recomp_bridge_t bridge={0};
  ee_state_t *s=start(0x00200000u,&bridge);
  ee_state_t before=*s;
  assert(ee_recomp_bridge_begin_native_window(&bridge,s->pc,arithmetic,4,4,0)==0);
  assert(!memcmp(s,&before,sizeof before));
  finish();
 }
 puts("PASS bridge native-window admission: live-source, device/event, budget, page and memory guards");
 return 0;
}
