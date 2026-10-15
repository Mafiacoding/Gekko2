/* BETA12.7: monitor counts only real INTC events and filtered guest reads. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../source/hw/ee_intc.c"
int main(void)
{
    uint32_t v=0;
    ee_intc_poll_trace_t t;
    ee_intc_init();
    ee_intc_raise(2);
    if(!ee_intc_mmio_read32(0x1000f000u,&v) || !(v & 4u))return 1;
    ee_intc_note_guest_stat_read(0x001fc974u,v);
    ee_intc_note_guest_stat_read(0x80001000u,v); /* unrelated consumer */
    ee_intc_poll_trace_snapshot(&t);
    if(t.stat_reads!=1||t.vblank_seen_reads!=1||t.vblank_clear_reads!=0||t.vblank_w1c)return 2;
    if(!ee_intc_mmio_write32(0x1000f000u,4u))return 3;
    ee_intc_mmio_read32(0x1000f000u,&v);
    ee_intc_note_guest_stat_read(0x001fc974u,v);
    ee_intc_poll_trace_snapshot(&t);
    if(t.stat_reads!=2||t.vblank_seen_reads!=1||t.vblank_clear_reads!=1||t.vblank_w1c!=1)return 4;
    ee_intc_raise(2);
    ee_intc_mmio_read32(0x1000f000u,&v);
    ee_intc_note_guest_stat_read(0x001fc974u,v);
    ee_intc_poll_trace_snapshot(&t);
    if(t.stat_reads!=3||t.vblank_seen_reads!=2||t.vblank_w1c!=1||ee_intc_get_raise_count(2)!=2)return 5;
    ee_intc_init();ee_intc_poll_trace_snapshot(&t);
    if(t.stat_reads||t.vblank_w1c)return 6;
    puts("PASS BETA12.7 INTC read/W1C/raise diagnostic does not alter guest register semantics");
    return 0;
}
