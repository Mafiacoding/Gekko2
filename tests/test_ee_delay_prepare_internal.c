#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "core/ee/ee_core.h"
static uint8_t ram[0x200100];
static uint32_t alu=(9u<<26)|(2u<<16)|77u;
static void word(unsigned offset,uint32_t v){for(unsigned n=0;n<4;n++)ram[offset+n]=(uint8_t)(v>>(8*n));}
static ee_state_t *setup(void)
{
 ee_state_t *st=ee_core_get_state();memset(st,0,sizeof(*st));st->ram=ram;st->ram_size=sizeof(ram);
 st->pc=0x80200004;st->next_pc=0x80200080;st->branch_pending=1;
 st->gpr[0].ud0=123;st->gpr[0].ud1=456;word(0x200004,alu);return st;
}
int main(void)
{
 ee_state_t *st=setup();assert(ee_core_block_prepare_delay(st,0x80200004,alu)==1);
 assert(st->pc==0x80200080&&st->next_pc==0x80200084&&!st->branch_pending);
 assert(st->exc_this_pc==0x80200004&&st->exc_in_delay_slot==1&&!st->mem_tlb_miss);
 assert(!st->gpr[0].ud0&&!st->gpr[0].ud1&&!st->instructions_executed);
 for(unsigned n=0;n<5;n++) {
  st=setup();
  if(n==0)st->branch_pending=0;
  if(n==1)st->pc+=4;
  if(n==2)st->halted=1;
  if(n==3)st->idle=1;
  if(n==4)word(0x200004,alu^1);
  ee_state_t before=*st;assert(!ee_core_block_prepare_delay(st,0x80200004,alu));assert(!memcmp(st,&before,sizeof(*st)));
 }
 st=setup();uint32_t load=(0x23u<<26)|(3u<<21)|(2u<<16);word(0x200004,load);st->gpr[3].ud0=0x80000000;
 assert(ee_core_block_prepare_delay(st,0x80200004,load)==1);
 st=setup();word(0x200004,load);st->gpr[3].ud0=0x00401000;ee_state_t before=*st;
 assert(!ee_core_block_prepare_delay(st,0x80200004,load));assert(!memcmp(st,&before,sizeof(*st)));
 assert(!ee_core_block_prepare_delay(NULL,0,0));
 puts("PASS compiled-delay preparation: target/BD/r0, source/annul/idle exits, live memory proof and atomic decline");return 0;
}
