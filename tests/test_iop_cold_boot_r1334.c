#include "core/iop/iop_core.c"
#include <assert.h>
int main(void)
{
 unsigned char rom[16]={0};bios_image_t bios={0};bios.data=rom;bios.size=sizeof rom;
 for(unsigned n=0;n<3;n++) {
  g_iop_spurious_istat_mask=0xffffffffu;g_iop_spurious_istat_hi_mask=0xffffffffu;
  s_zero_run=7;iop_block_stale=19;
  assert(iop_core_init(&bios)==0);
  assert(!g_iop_spurious_istat_mask&&!g_iop_spurious_istat_hi_mask&&!s_zero_run&&!iop_block_stale&&!iop_before_tick);
  assert(!g_iop.idle&&!g_iop.halted&&!g_iop.instructions_executed&&!g_iop.sched_ticks);
  iop_core_shutdown();
 }
 puts("PASS repeated IOP cold boot clears spurious IRQ acknowledgement, zero-run and stale counters");return 0;
}
