#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "core/ee/ee_core.h"
static uint8_t ram[0x200080];
int main(void)
{
 const uint64_t values[]={0,1,UINT64_MAX,0x100000000ull};
 for(unsigned sub=0x10;sub<=0x13;sub++)for(unsigned r=0;r<3;r++)for(unsigned n=0;n<4;n++) {
  unsigned rs=r==0?0:r==1?3:31;ee_state_t *st=ee_core_get_state();memset(st,0,sizeof(*st));st->ram=ram;st->ram_size=sizeof(ram);
  st->pc=0x80200000;st->next_pc=st->pc+4;st->gpr[rs].ud0=values[n];st->gpr[31].ud1=0x1122334455667788ull;
  uint32_t iw=(1u<<26)|(rs<<21)|(sub<<16)|7;for(unsigned k=0;k<4;k++)ram[0x200000+k]=(uint8_t)(iw>>(8*k));
  uint64_t input=rs==0?0:rs==31?0x80200008:values[n];int take=(int64_t)input<0;if(sub&1)take=!take;
  assert(ee_core_step_n(1)==1);assert(st->gpr[31].ud0==0x80200008&&st->gpr[31].ud1==0x1122334455667788ull);
  if(sub<0x12||take){assert(st->pc==0x80200004&&st->branch_pending);assert(st->next_pc==(take?0x80200020:0x80200008));}
  else assert(st->pc==0x80200008&&!st->branch_pending&&st->next_pc==0x8020000c);
 }
 puts("PASS 48 REGIMM link outcomes: unconditional link, link-before-condition rs31, zero, sign and likely annulment");return 0;
}
