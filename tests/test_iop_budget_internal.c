#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "core/iop/iop_core.h"
#include "core/recompiler/iop_jit.h"
static uint8_t ram[0x100200];
static unsigned calls;
static void before_tick(void){calls++;}
int main(void)
{
 iop_state_t *st=iop_core_get_state();
 for(unsigned n=0;n<128;n++) {
  uint32_t w=(9u<<26)|(2u<<21)|(2u<<16)|1u;
  for(unsigned k=0;k<4;k++)ram[0x100000+n*4+k]=(uint8_t)(w>>(k*8));
 }
 const unsigned budgets[]={0,1,2,7,8,9,17,64};
 for(unsigned idle=0;idle<2;idle++)for(unsigned n=0;n<sizeof(budgets)/sizeof(budgets[0]);n++) {
  memset(st,0,sizeof(*st));st->ram=ram;st->ram_size=sizeof(ram);st->pc=0x80100000;st->next_pc=st->pc+4;st->idle=idle;
  calls=0;unsigned b=budgets[n];
  assert(iop_core_step_interleaved_n(b,before_tick)==b);
  assert(calls==b&&st->sched_ticks==b);
  assert(st->instructions_executed==(idle?0:b)&&st->gpr[2]==(idle?0:b));
 }
 st->halted=1;calls=0;assert(iop_core_step_interleaved_n(8,before_tick)==0&&calls==0);
 iop_jit_reset_for_test();puts("PASS IOP tick budgets, callback cadence, idle and halt");return 0;
}
