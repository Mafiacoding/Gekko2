/* Regression for generalising the proven Crash E3 VBlank poll to other EE games.
 * Absolutely no PC-only shortcut: full side-effect-free loop recognition. */
#include <stdio.h>
#include <string.h>
#include "core/ee/ee_core.c"
static void put(ee_state_t *s, uint32_t base, uint32_t off, uint32_t value)
{ ee_mem_write32(s,0x80000000u+base+off,value); }
static void poll(ee_state_t *s,uint32_t base,unsigned r,unsigned nops)
{
  put(s,base,0,0x3c001001u|(r<<16));
  put(s,base,4,0x8c00f000u|(r<<21)|(r<<16));
  put(s,base,8,0x30000004u|(r<<21)|(r<<16));
  for(unsigned j=0;j<nops;j++)put(s,base,12+4*j,0);
  const uint32_t branch=12+4*nops;
  put(s,base,branch,0x10000000u|(r<<21)|((uint16_t)(-4-(int32_t)nops)));
  put(s,base,branch+4,0);
}
int main(void)
{
#ifndef GEKKO2_IDLE_SKIP
 puts("SKIP unavailable");return 77;
#else
 bios_image_t bios={0}; if(ee_core_init(&bios))return 1;
 ee_state_t *s=ee_core_get_state();ee_intc_init();
 poll(s,0x001fc970u,2,3);
 if(!ee_vblank_poll_signature(s,0x001fc974u))return 2;
 /* A different executable PC and GPR (e.g. BIOS, Tekken or GT3) */
 poll(s,0x0020a000u,5,1);
 if(!ee_vblank_poll_signature(s,0x0020a00cu))return 3;
 if(ee_vblank_poll_signature(s,0x0020a020u))return 4;
 put(s,0x0020a000u,12,0xac600000u); /* SW has an observable side effect */
 if(ee_vblank_poll_signature(s,0x0020a004u))return 5;
 poll(s,0x0020a000u,5,1);
 put(s,0x0020a000u,16,0x10a0fffeu); /* Incorrect branch target */
 if(ee_vblank_poll_signature(s,0x0020a004u))return 6;
 poll(s,0x0020a000u,5,1);
 put(s,0x0020a000u,20,0xac000000u); /* Side-effectful delay slot */
 if(ee_vblank_poll_signature(s,0x0020a004u))return 7;
 poll(s,0x0020a000u,5,1);
 ee_intc_raise(2);
 if(ee_vblank_poll_signature(s,0x0020a004u))return 8;
 ee_intc_mmio_write32(0x1000f000u,4);
 s->pc=0x0020a004u;s->next_pc=s->pc+4u;s->cop0[12]=0x70030c11u;
 g_ee_display_phase=1024u;g_ee_display_ticks=1024u;
 ee_timers_batch_clock[0]=2000u;
 s->cop0[9]=200u;s->cop0[11]=0x70000000u;
 unsigned n=ee_idle_skip(s,64);
 if(!n||ee_core_vblank_fast_wait_stat(0)!=1||ee_core_vblank_fast_wait_stat(1)!=n||ee_core_vblank_fast_wait_stat(2)!=0x0020a000u)return 9;
 ee_intc_raise(2);
 if(ee_idle_skip(s,64))return 10;
 /* Hoisted LUI case: live $s0 points to MMIO and loop is LW/ANDI/BEQZ/NOP. */
 put(s,0x0020b000u,0,0x8e030000u); /* LW $v1,0($s0) */
 put(s,0x0020b000u,4,0x30630004u); /* ANDI $v1,$v1,4 */
 put(s,0x0020b000u,8,0x1060fffdu); /* BEQZ $v1,LW */
 put(s,0x0020b000u,12,0);
 s->gpr[16].ud0=0x1000f000u;
 ee_intc_mmio_write32(0x1000f000u,4);
 if(!ee_vblank_poll_signature(s,0x0020b004u))return 11;
 if(ee_core_vblank_fast_wait_stat(2)!=0x0020b000u)return 12;
 s->gpr[16].ud0=0x1000f004u;
 if(ee_vblank_poll_signature(s,0x0020b004u))return 13;
 s->gpr[16].ud0=0x1000f000u;
 put(s,0x0020b000u,12,0xac000000u);
 if(ee_vblank_poll_signature(s,0x0020b004u))return 14;
 put(s,0x0020b000u,12,0);
 ee_intc_raise(2);
 if(ee_vblank_poll_signature(s,0x0020b004u))return 15;
 puts("PASS original and hoisted register-based VBlank polling, guard failures, event deadlines");
 return 0;
#endif
}
