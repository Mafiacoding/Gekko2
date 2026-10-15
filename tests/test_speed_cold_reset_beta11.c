#define GEKKO2_IDLE_SKIP 1
#define GEKKO2_SPEED_STUB 1
#define GEKKO2_SPEED_RETIRE 1
#include "core/ee/ee_core.c"
#include <assert.h>
int main(void){
 bios_image_t bios={0};assert(!ee_core_init(&bios));
 ee_quiet_left=9;ee_quiet_tlb[0]=1;ee_fastmem_invalidate();
 assert(!ee_quiet_left&&!ee_quiet_tlb[0]);
 ee_quiet_left=7;ee_quiet_tlb[0]=1;ee_spin_tab[0].hits=99;
 ee_idle_on=1;ee_spin_tick=8;ee_dma_diag_hits=9;ee_arm_why[3]=12;
 ee_core_shutdown();assert(!ee_core_init(&bios));
 assert(!ee_quiet_left&&!ee_quiet_tlb[0]&&!ee_spin_tab[0].hits);
 assert(!ee_idle_on&&!ee_spin_tick&&!ee_dma_diag_hits&&!ee_arm_why[3]);
 ee_core_shutdown();puts("PASS cold boot clears quiet budgets, TLB proofs and spin history");return 0;
}
