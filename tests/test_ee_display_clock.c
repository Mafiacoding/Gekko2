/* Guest MTC0 Count and its 32-bit wrap must not move display edges. */
#include <stdio.h>
#include "core/ee/ee_core.c"
static unsigned failures;
#define CHECK(x,m) do { if(!(x)){printf("FAIL: %s\n",m);failures++;} } while(0)
static void clear_edges(void){ee_intc_get_state()->stat=0;gs_get_state()->csr&=~8ull;}
int main(void){
 bios_image_t bios={0};if(system_init(&bios,&bios))return 2;
 ee_state_t *s=ee_core_get_state();s->cop0[12]=0;
 /* A genuine MTC0 $t0,Count followed by NOPs in mapped kernel RAM. */
 s->pc=0x80010000;s->next_pc=s->pc+4;s->gpr[8].ud0=0;
 ee_mem_write32(s,s->pc,0x40884800u);ee_mem_write32(s,s->pc+4,0);
 ee_mem_write32(s,s->pc+8,0);ee_mem_write32(s,s->pc+12,0);
 ee_core_display_clock_load(EE_CYCLES_PER_FRAME_NTSC-2);clear_edges();
 ee_core_step();
 CHECK(s->cop0[9]==1,"guest MTC0 Count really executes");
 CHECK(ee_core_display_clock_save()==EE_CYCLES_PER_FRAME_NTSC-1,"Count write preserves display phase");
 CHECK(!(ee_intc_get_state()->stat&4),"no premature VBLANK after Count reset");
 ee_core_step();
 CHECK((ee_intc_get_state()->stat&4)&&(gs_get_state()->csr&8),"VBLANK and GS VSYNC arrive at independent deadline");
 clear_edges();s->pc=0x80010000;s->next_pc=s->pc+4;s->gpr[8].ud0=EE_CYCLES_PER_FRAME_NTSC-1;
 ee_core_display_clock_load(100);ee_core_step();
 CHECK(s->cop0[9]==EE_CYCLES_PER_FRAME_NTSC,"Count reaches an apparent frame edge");
 CHECK(!(ee_intc_get_state()->stat&4)&&!(gs_get_state()->csr&8),"Count write cannot manufacture a display edge");
 ee_core_display_clock_load(EE_CYCLES_VBLANK_DURATION-1);clear_edges();
 uint64_t retired=s->instructions_executed;s->cop0[9]=0xffffffffu;ee_core_park_tick(s);
 CHECK(s->cop0[9]==0,"parked Count wraps independently");
 CHECK(ee_intc_get_state()->stat&8,"parked clock emits VBLANK end");
 CHECK(s->instructions_executed==retired,"parked display timing does not retire guest instructions");
 ee_core_display_clock_load(0xffffffffull);s->cop0[9]=0;clear_edges();ee_core_park_tick(s);
 CHECK(ee_core_display_clock_save()==0x100000000ull,"display clock continues beyond 32-bit Count width");
 /* Restore a high 64-bit clock at the same hardware edge, then wrap it. */
 uint64_t high=((1ull<<48)/EE_CYCLES_PER_FRAME_NTSC)*EE_CYCLES_PER_FRAME_NTSC;
 ee_core_display_clock_load(high+EE_CYCLES_PER_FRAME_NTSC-1);clear_edges();ee_core_park_tick(s);
 CHECK((ee_intc_get_state()->stat&4)&&(gs_get_state()->csr&8),"high checkpoint clock preserves frame edge");
 clear_edges();ee_core_park_tick(s);
 CHECK(!(ee_intc_get_state()->stat&4)&&!(gs_get_state()->csr&8),"restored frame edge is not repeated");
 ee_core_display_clock_load(~0ull);clear_edges();ee_core_park_tick(s);
 CHECK(ee_core_display_clock_save()==0,"64-bit display oscillator wraps");
 CHECK((ee_intc_get_state()->stat&4)&&(gs_get_state()->csr&8),"64-bit wrap preserves original modulo-zero edge");
 ee_core_init(&bios);CHECK(ee_core_display_clock_save()==0,"cold reset resets display clock");
 printf("Independent display clock: %u failures\n",failures);return failures?1:0;
}
