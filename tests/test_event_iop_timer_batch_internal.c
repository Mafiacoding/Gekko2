#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "core/hw/iop_intc.h"
static uint32_t irq[32];
static void timer_test_raise(int bit){irq[bit]++;}
#define iop_intc_raise timer_test_raise
#include "hw/iop_timers.c"
#undef iop_intc_raise
#define CHECK(x) do{if(!(x)){printf("FAIL line %d\n",__LINE__);return 1;}}while(0)
static uint32_t seed=1309;
static uint32_t rnd(void){seed=seed*1664525u+1013904223u;return seed;}
int main(void){
    iop_timers_state_t expected,actual;
    uint32_t expected_irq[32]={0},live_irq[32];
    iop_timers_init();expected=g_timers;
    for(unsigned round=0;round<30000;round++) {
        iop_timers_snapshot(&actual);memcpy(live_irq,irq,sizeof(irq));
        unsigned timer=rnd()%6,reg=rnd()%3;
        uint32_t value=rnd();
        if(reg==0 && round%7)value&=0xffffu;
        if(reg==1)value&=0x63ffu;
        if(reg==2 && round%11)value&=0xffffu;
        unsigned addr=s_ranges[timer].base+reg*4u;
        g_timers=expected;memcpy(irq,expected_irq,sizeof(irq));
        iop_timers_mmio_write32(addr,value);expected=g_timers;
        g_timers=actual;memcpy(irq,live_irq,sizeof(irq));
        iop_timers_mmio_write32(addr,value);
        iop_timers_snapshot(&actual);
        unsigned ticks=1+rnd()%128;
        uint32_t trace[128][32];
        g_timers=expected;memcpy(irq,expected_irq,sizeof(irq));
        for(unsigned n=0;n<ticks;n++){iop_timers_tick_scalar();memcpy(trace[n],irq,sizeof(irq));}
        expected=g_timers;memcpy(expected_irq,irq,sizeof(irq));
        g_timers=actual;memcpy(irq,live_irq,sizeof(irq));g_event_distance=0;
        for(unsigned n=0;n<ticks;n++) {
            iop_timers_tick();CHECK(!memcmp(irq,trace[n],sizeof(irq)));
            if(n%17==0){uint32_t v;CHECK(iop_timers_mmio_read32(addr|0x80000000u,&v));}
        }
        iop_timers_snapshot(&actual);CHECK(!memcmp(&expected,&actual,sizeof(actual)));
    }
    /* Boundary wrap and target==0 retain the original model, including
     * its 32-bit overflow semantics. Snapshot/restore cannot leave an
     * event distance derived from a different state behind. */
    for(unsigned timer=0;timer<6;timer++)for(unsigned k=0;k<4;k++) {
        iop_timers_init();memset(irq,0,sizeof(irq));
        g_timers.t[timer].count=k<2?0xffffffffu:0xfffeu;
        g_timers.t[timer].target=k&1?0:0xffffu;
        g_timers.t[timer].mode=IOP_CNT_MODE_INTR_ENABLE|IOP_CNT_MODE_TARGET_INTR|
            IOP_CNT_MODE_OVERFL_INTR|IOP_CNT_MODE_REPEAT_INTR|(k&1?IOP_CNT_MODE_ZERO_RETURN:0);
        expected=g_timers;memset(expected_irq,0,sizeof(expected_irq));
        for(unsigned n=0;n<40;n++) {
            iop_timers_snapshot(&actual);memcpy(live_irq,irq,sizeof(irq));
            g_timers=expected;memcpy(irq,expected_irq,sizeof(irq));
            iop_timers_tick_scalar();expected=g_timers;memcpy(expected_irq,irq,sizeof(irq));
            g_timers=actual;memcpy(irq,live_irq,sizeof(irq));
            iop_timers_tick();iop_timers_snapshot(&actual);
            CHECK(!memcmp(&expected,&actual,sizeof(actual)));CHECK(!memcmp(irq,expected_irq,sizeof(irq)));
        }
    }
    iop_timers_init();
    for(unsigned timer=0;timer<6;timer++)g_timers.t[timer].target=100;
    iop_timers_tick();CHECK(g_deferred_ticks==1);
    iop_timers_snapshot(&actual);CHECK(actual.t[0].count==1);
    actual.t[0].count=99;iop_timers_restore(&actual);iop_timers_tick();
    iop_timers_snapshot(&actual);CHECK(actual.t[0].count==100&&(actual.t[0].mode&IOP_CNT_MODE_TARGET_FLAG));
    iop_timers_state_t *held=iop_timers_get_state();
    held->t[0].mode=IOP_CNT_MODE_STOPPED;held->t[0].count=8;iop_timers_tick();CHECK(held->t[0].count==8);
    held->t[0].mode=IOP_CNT_MODE_TARGET_FLAG;iop_timers_tick();CHECK(held->t[0].count==9);
    puts("PASS 30000 IOP timer MMIO/tick transactions, per-tick IRQ counts, wrap, snapshots and held pointers");return 0;
}
