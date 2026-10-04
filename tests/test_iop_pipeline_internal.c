#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "core/iop/iop_core.h"
#include "core/hw/iop_intc.h"
#include "core/hw/iop_timers.h"
#include "core/hw/iop_hle_thread.h"
#include "core/hw/iop_asyncio.h"
static uint8_t ram[0x200000];
static iop_state_t *st;
static void put(unsigned off,uint32_t w){for(unsigned k=0;k<4;k++)ram[off+k]=(uint8_t)(w>>(8*k));}
static void setup(void){st=iop_core_get_state();memset(st,0,sizeof(*st));memset(ram,0,sizeof(ram));st->ram=ram;st->ram_size=sizeof(ram);st->pc=0x80100000;st->next_pc=st->pc+4;st->gpr[1]=0x80180000;st->gpr[2]=9;iop_intc_init();iop_timers_init();iop_hle_thread_init();iop_asyncio_init();put(0x180000,0x12345678);put(0x180004,0x90abcdef);}
static void step(void){assert(iop_core_step()==0);}
int main(void)
{
 setup();put(0x100000,(0x23u<<26)|(1u<<21)|(2u<<16));put(0x100004,(9u<<26)|(2u<<21)|(3u<<16)|1);put(0x100008,(9u<<26)|(2u<<21)|(4u<<16)|2);
 step();assert(st->gpr[2]==9&&st->load_delay_reg==2&&st->load_delay_value==0x12345678);step();assert(st->gpr[3]==10&&st->gpr[2]==0x12345678&&!st->load_delay_reg);step();assert(st->gpr[4]==0x1234567a);
 setup();put(0x100000,(0x23u<<26)|(1u<<21)|(2u<<16));put(0x100004,(9u<<26)|(2u<<16)|7);step();step();assert(st->gpr[2]==7&&!st->load_delay_reg);
 setup();put(0x100000,(0x23u<<26)|(1u<<21)|(2u<<16));put(0x100004,(0x23u<<26)|(1u<<21)|(2u<<16)|4);put(0x100008,(9u<<26)|(2u<<21)|(3u<<16)|1);step();step();assert(st->gpr[2]==9&&st->load_delay_value==0x90abcdef);step();assert(st->gpr[3]==10&&st->gpr[2]==0x90abcdef);
 setup();st->gpr[2]=0x80180001;put(0x100000,(0x22u<<26)|(2u<<21)|(2u<<16)|3);put(0x100004,(0x26u<<26)|(2u<<21)|(2u<<16));put(0x100008,0);step();step();assert(st->gpr[2]==0x80180001);step();assert(st->gpr[2]==0xef123456);
 setup();st->cop0[12]=0x1234;put(0x100000,(0x10u<<26)|(2u<<16)|(12u<<11));put(0x100004,(9u<<26)|(2u<<21)|(3u<<16)|1);step();assert(st->gpr[2]==9);step();assert(st->gpr[3]==10&&st->gpr[2]==0x1234);
 for(unsigned take=0;take<2;take++)for(unsigned bev=0;bev<2;bev++)for(unsigned fault=0;fault<4;fault++) {
  setup();st->gpr[2]=0;st->gpr[3]=take?0:1;st->cop0[12]=(bev?0x400000u:0u)|0xf;st->cop0[13]=0x400;
  put(0x100000,(4u<<26)|(3u<<21)|7);
  uint32_t w=fault==0?12u:fault==1?0xfc000000u:fault==2?((0x23u<<26)|(1u<<21)|(4u<<16)|1):((8u<<26)|(5u<<21)|(6u<<16)|1);
  st->gpr[5]=0x7fffffff;put(0x100004,w);step();assert(st->branch_delay_pending);step();
  unsigned code=fault==0?8:fault==1?10:fault==2?4:12;
  assert(st->cop0[14]==0x80100000&&(st->cop0[13]&0x8000007cu)==(0x80000000u|(code<<2)));
  assert(st->pc==(bev?0xbfc00180u:0x80000080u)&&st->next_pc==st->pc+4);
  assert(st->cop0[6]==(take?0x80100020u:0x80100008u)&&!st->branch_delay_pending);
  if(fault==2)assert(st->cop0[8]==0x80180001);
  if(!bev && fault<2){step();assert(st->pc==(take?0x80100020u:0x80100008u)&&!st->exception_pending);}
 }
 for(unsigned op=0;op<4;op++) {
  setup();uint32_t w=op==0?((0x21u<<26)|(1u<<21)|(2u<<16)|1):op==1?((0x2bu<<26)|(1u<<21)|(2u<<16)|2):op==2?((8u<<26)|(5u<<21)|1):((5u<<21)|(6u<<16)|0x22u);
  st->gpr[5]=op==3?0x80000000u:0x7fffffffu;st->gpr[6]=1;st->cop0[13]=0x80000400u;put(0x100000,w);step();
  assert(st->cop0[14]==0x80100000&&!(st->cop0[13]&0x80000000u));assert((st->cop0[13]&0x7cu)==((op==0?4:op==1?5:12)<<2));
 }
 setup();st->load_delay_reg=2;st->load_delay_value=0x55667788;st->pc=0x80100003;st->next_pc=st->pc+4;step();assert(st->cop0[8]==0x80100003&&st->cop0[14]==0x80100003&&st->gpr[2]==0x55667788&&!st->load_delay_reg);
 setup();put(0x100000,(0x23u<<26)|(1u<<21)|(2u<<16));st->cop0[12]=0x401;iop_mem_write32(st,0x1f801074,1);iop_mem_write32(st,0x1f801078,1);iop_mem_write32(st,0x1f801070,0xffffffff); /* raise through vblank tick 0 */
 st->sched_ticks=615185;step();assert(st->gpr[2]==0x12345678&&!st->load_delay_reg&&st->exception_pending);
 puts("PASS IOP independent load-delay, merge forwarding, explicit-write cancellation, EPC/BD/TAR, alignment, overflow and IRQ flush oracles");return 0;
}
