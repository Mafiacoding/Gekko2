#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "hw/ee_timers.c"
static uint32_t seed=12345;
static uint32_t rnd(void){seed=seed*1664525u+1013904223u;return seed;}
#define CHECK(x) do{if(!(x)){printf("FAIL line %d\n",__LINE__);return 1;}}while(0)
int main(void){
    ee_timers_state_t expected,actual;
    uint64_t clock=0;uint32_t irq[4]={0};
    ee_timers_init();memset(&expected,0,sizeof(expected));
    for(unsigned round=0;round<30000;round++) {
        if(round%3==0) {
            unsigned i=rnd()%4,reg=rnd()%3;uint32_t value=rnd();
            if(reg==0)value&=0xffffu;
            if(reg==1)value=(value&3u)|EE_CNT_MODE_CUE|EE_CNT_MODE_CMP_ENABLE|EE_CNT_MODE_OVF_ENABLE|(value&EE_CNT_MODE_ZERO_RETURN);
            if(reg==2)value&=0xffffu;
            /* Run the same actual MMIO operation against independent states. */
            ee_timers_snapshot(&actual);
            uint64_t liveclock=g_bus_tick_counter;uint32_t liveirq[4];memcpy(liveirq,g_irq_count,sizeof(irq));
            g_timers=expected;g_bus_tick_counter=clock;memcpy(g_irq_count,irq,sizeof(irq));
            ee_timers_mmio_write32(s_ranges[i].base+reg*16,value);expected=g_timers;
            g_timers=actual;g_bus_tick_counter=liveclock;memcpy(g_irq_count,liveirq,sizeof(irq));
            ee_timers_mmio_write32(s_ranges[i].base+reg*16,value);
        }
        unsigned ticks=1+rnd()%160;
        /* Scalar original implementation supplies complete expected state. */
        ee_timers_snapshot(&actual);uint64_t liveclock=g_bus_tick_counter;
        uint32_t liveirq[4];memcpy(liveirq,g_irq_count,sizeof(irq));
        uint32_t distance=g_event_distance;
        g_timers=expected;g_bus_tick_counter=clock;memcpy(g_irq_count,irq,sizeof(irq));
        uint32_t trace[160][4];
        for(unsigned n=0;n<ticks;n++){ee_timers_tick_scalar();memcpy(trace[n],g_irq_count,sizeof(irq));}
        expected=g_timers;clock=g_bus_tick_counter;memcpy(irq,g_irq_count,sizeof(irq));
        g_timers=actual;g_bus_tick_counter=liveclock;memcpy(g_irq_count,liveirq,sizeof(irq));g_event_distance=distance;
        for(unsigned n=0;n<ticks;n++) {
            ee_timers_tick();CHECK(!memcmp(trace[n],g_irq_count,sizeof(irq)));
            if(n%17==0){uint32_t dummy;ee_timers_mmio_read32(0x10000000,&dummy);}
        }
        ee_timers_snapshot(&actual);CHECK(!memcmp(&actual,&expected,sizeof(actual)));CHECK(clock==g_bus_tick_counter);
    }
    CHECK(g_batched_ticks>g_boundary_ticks*10);
    printf("PASS 30000 mixed timer transactions; batched=%llu boundaries=%llu\n",(unsigned long long)g_batched_ticks,(unsigned long long)g_boundary_ticks);
    /* Divider wrap, invalid wide count, COMP=65536 and legacy mutations. */
    for(unsigned mode=0;mode<4;mode++)for(unsigned k=0;k<3;k++) {
        ee_timers_init();g_bus_tick_counter=UINT64_MAX-6;
        g_timers.t[0].mode=EE_CNT_MODE_CUE|EE_CNT_MODE_ZERO_RETURN|mode;
        g_timers.t[0].count=k==0?0xffffu:k==1?0xffffffffu:0;
        g_timers.t[0].comp=k==2?0x10000u:3;
        expected=g_timers;clock=g_bus_tick_counter;
        for(unsigned n=0;n<40;n++) {
            actual=g_timers;uint64_t live=g_bus_tick_counter;uint32_t distance=g_event_distance,pending=g_deferred_ticks;
            g_timers=expected;g_bus_tick_counter=clock;ee_timers_tick_scalar();expected=g_timers;clock=g_bus_tick_counter;
            g_timers=actual;g_bus_tick_counter=live;g_event_distance=distance;g_deferred_ticks=pending;
            ee_timers_tick();ee_timers_snapshot(&actual);CHECK(!memcmp(&expected,&actual,sizeof(actual)));CHECK(clock==g_bus_tick_counter);
        }
    }
    ee_timers_init();ee_timers_state_t *mutable=ee_timers_get_state();
    ee_timers_tick();mutable->t[0].mode=EE_CNT_MODE_CUE;mutable->t[0].count=8;
    ee_timers_tick();CHECK(mutable->t[0].count==9);
    puts("PASS wrap, wide COUNT/COMP and retained mutable state");return 0;
}
