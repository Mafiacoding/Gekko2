/* White-box host test for BETA12.7 exact-poll-signature acceleration. */
#include <stdio.h>
#include <string.h>
#include "core/ee/ee_core.c"
int main(void) {
#ifndef GEKKO2_IDLE_SKIP
 puts("SKIP feature inactive: build with -DGEKKO2_IDLE_SKIP");return 77;
#else
 bios_image_t bios={0};
 if (ee_core_init(&bios))return 1;
 ee_state_t *s=ee_core_get_state();
 const uint32_t words[]={0x3c021001,0x8c42f000,0x30420004,0,0,0,0x1040fff9,0};
 for(unsigned i=0;i<sizeof(words)/sizeof(words[0]);i++)ee_mem_write32(s,0x80000000u+0x001fc970u+4*i,words[i]);
 ee_intc_init();
 if(!ee_vblank_poll_signature(s,0x001fc974u))return 2;
 if(ee_vblank_poll_signature(s,0x001fc960u))return 3;
 ee_intc_raise(2);
 if(ee_vblank_poll_signature(s,0x001fc974u))return 4;
 ee_intc_mmio_write32(0x1000f000u,4);
 if(!ee_vblank_poll_signature(s,0x001fc974u))return 5;
 ee_mem_write32(s,0x80000000u+0x001fc988u,0);
 if(ee_vblank_poll_signature(s,0x001fc974u))return 6;
 ee_mem_write32(s,0x80000000u+0x001fc988u,0x1040fff9u);
 s->pc=0x001fc974u;s->next_pc=s->pc+4u;s->cop0[12]=0x70030c11u;
 g_ee_display_phase=1024u;g_ee_display_ticks=1024;
 ee_timers_batch_clock[0]=2000u;
 s->cop0[9]=200u;s->cop0[11]=0x70000000u;
 unsigned n=ee_idle_skip(s,64);
 if(n==0||ee_core_vblank_fast_wait_stat(0)!=1||ee_core_vblank_fast_wait_stat(1)!=n) {
  printf("SKIP n=%u fast=%llu slots=%llu phase=%u\n",n,(unsigned long long)ee_core_vblank_fast_wait_stat(0),(unsigned long long)ee_core_vblank_fast_wait_stat(1),g_ee_display_phase);return 7;
 }
 if(g_ee_display_phase!=1024u+n)return 8;
 ee_intc_raise(2);
 if(ee_idle_skip(s,64)!=0)return 9;
 puts("PASS exact INTC poll signature skips only while real bit2 is clear, with bounded hardware clocks");return 0;
#endif
}
