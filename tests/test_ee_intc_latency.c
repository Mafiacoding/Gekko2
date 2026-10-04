/* Deferred INTC delivery must preserve the peripheral polling window. */
#include <stdio.h>
#include <string.h>
#include "core/ee/ee_core.c"
static unsigned failures;
#define CHECK(x,m) do {if(!(x)){printf("FAIL: %s\n",m);failures++;}}while(0)
static void prepare(void) {
    memset(&g_state,0,sizeof(g_state)); memset(&g_ee_irq,0,sizeof(g_ee_irq));
    ee_intc_init(); g_state.cop0[12]=0x00010401u;
    ee_intc_get_state()->mask=4; ee_intc_raise(2);
}
int main(void) {
    prepare(); ee_check_intc_interrupt(&g_state,0x1000);
    CHECK((g_state.cop0[13]&EE_CAUSE_IP2)!=0,"IP2 reflects live line immediately");
    CHECK(!(g_state.cop0[12]&2),"assertion does not immediately vector");
    for(unsigned i=1;i<=4;i++) {
        /* Guest Count writes cannot change the delivery deadline. */
        g_state.cop0[9]=i&1?0xffffffffu:0;
        ee_irq_tick(); ee_check_intc_interrupt(&g_state,0x1000+4*i);
        CHECK(!!(g_state.cop0[12]&2)==(i==4),"four independent timing ticks");
    }
    CHECK(g_state.cop0[14]==0x1010,"EPC is genuine delivery boundary");
    prepare();ee_check_intc_interrupt(&g_state,0x2000);
    ee_irq_tick();ee_intc_mmio_write32(0x1000f000,4);
    ee_check_intc_interrupt(&g_state,0x2004);
    CHECK(!(g_state.cop0[13]&EE_CAUSE_IP2)&&!g_ee_irq.intc_armed,"ack cancels pending delivery");
    for(unsigned i=0;i<8;i++){ee_irq_tick();ee_check_intc_interrupt(&g_state,0x2008);}
    CHECK(!(g_state.cop0[12]&2),"cancelled IRQ never vectors");
    prepare();g_state.cop0[12]&=~EE_STATUS_IM2;
    ee_check_intc_interrupt(&g_state,0x3000);
    for(unsigned i=0;i<4;i++){ee_irq_tick();ee_check_intc_interrupt(&g_state,0x3004);}
    CHECK(!(g_state.cop0[12]&2),"CPU mask gates mature IRQ");
    g_state.cop0[12]|=EE_STATUS_IM2;ee_check_intc_interrupt(&g_state,0x3010);
    CHECK(g_state.cop0[14]==0x3010,"unmask delivers mature pending line");
    prepare();ee_check_intc_interrupt(&g_state,0x4000);ee_irq_tick();
    ee_irq_checkpoint_t saved;ee_core_irq_checkpoint_save(&saved);
    ee_irq_tick();ee_core_irq_checkpoint_load(&saved);
    CHECK(g_ee_irq.intc_delay==3&&g_ee_irq.intc_armed==1,"checkpoint retains partial delay");
    for(unsigned i=0;i<3;i++){ee_irq_tick();ee_check_intc_interrupt(&g_state,0x4010);}
    CHECK(g_state.cop0[14]==0x4010,"restored countdown delivers exactly once");
    /* Execute a real MIPS polling loop and a real ACK/ERET handler.
     * The event is asserted before a NOP immediately preceding LW.
     * Zero-delay delivery loses the flag before LW; deferred delivery
     * lets the loaded value drive the guest's own branch. */
    bios_image_t bios = {0};
    CHECK(ee_core_init(&bios)==0,"initialize executable polling fixture");
    ee_state_t *st=ee_core_get_state();
    uint32_t code[]={0,0x8d020000u,0x30420004u,0x14400003u,0,
                     0x08000401u,0,0x24100001u,0x08000408u,0};
    for(unsigned i=0;i<sizeof(code)/sizeof(code[0]);i++)
        ee_mem_write32(st,0x80001000u+4*i,code[i]);
    uint32_t handler[]={0x3c1b1000u,0x377bf000u,0x241a0004u,
                        0xaf7a0000u,0x42000018u,0};
    for(unsigned i=0;i<sizeof(handler)/sizeof(handler[0]);i++)
        ee_mem_write32(st,0x80000200u+4*i,handler[i]);
    st->pc=0x80001000u;st->next_pc=st->pc+4;
    st->cop0[12]=0x10401u;st->gpr[8].ud0=0x1000f000u;
    ee_intc_get_state()->mask=4;ee_intc_raise(2);
    for(unsigned i=0;i<100&&!st->halted;i++)ee_core_step();
    CHECK(st->gpr[16].ud0==1,"guest polling branch exits without forced PC/status");
    CHECK(!(ee_intc_get_state()->stat&4),"guest handler acknowledges real event");
    CHECK(!st->halted,"polling fixture keeps running");
    printf("INTC latency: %u failures\n",failures);return failures?1:0;
}
