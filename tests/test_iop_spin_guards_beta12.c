#define GEKKO2_IDLE_SKIP 1
#include "core/iop/iop_core.c"
#include <assert.h>
int main(void){
 bios_image_t bios={0};assert(!iop_core_init(&bios));
 gekko2_optimization_mask|=GEKKO2_OPT_BIT(GEKKO2_OPT_IOP_DEADLINES);
 iop_state_t *s=&g_iop;iop_hle_thread_init();
 s->pc=0x19230;s->next_pc=0x19234;s->gpr[16]=0x2000;
 s->ram[0x19230]=0x0f;s->ram[0x19231]=0x64;s->ram[0x19232]=0;s->ram[0x19233]=0x0c;
 assert(!iop_spin_pc());
 s->gpr[31]=0x19230;assert(iop_hle_thread_try_handle(s,IOP_HLE_THREAD_GETTHREADID));
 s->pc=0x19230;s->next_pc=0x19234;assert(iop_spin_pc());
 s->gpr[16]=0xfffffff8u;assert(!iop_spin_pc());s->gpr[16]=0x2000;
 s->gpr[4]=100;assert(iop_hle_thread_try_handle(s,IOP_HLE_THREAD_DELAYTHREAD));
 s->pc=0x19230;s->next_pc=0x19234;assert(!iop_spin_pc());
 iop_core_shutdown();puts("PASS IOP spin: bootstrap, deadline and wrapped-address guards");return 0;
}
