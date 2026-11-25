/* Synthetic legal C.F.S and reserved-opcode architectural exceptions. */
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/recompiler/optimization.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static void put(uint8_t *p,uint32_t w){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(w>>(8*i));}
int main(void)
{
 bios_image_t b={0};assert(!system_init(&b,&b));ee_state_t *s=ee_core_get_state(),initial=*s;
 uint32_t cf=(17u<<26)|(16u<<21)|(2u<<16)|(3u<<11)|0x30u,pc=0x80200000u;
 uint32_t values[]={0,0x80000000u,0x7f800000u,0x7fc00000u,0xffffffffu,0x00800000u,0x12345678u};
 unsigned checks=0;
 for(unsigned a=0;a<7;a++)for(unsigned d=0;d<7;d++){
  *s=initial;s->pc=pc;s->next_pc=pc+4;s->cop0[12]=0;s->fpr[2]=values[a];s->fpr[3]=values[d];s->fcr31=values[a]^0x00ffffffu;
  uint32_t before=s->fcr31;put(s->ram+0x200000,cf);ee_reg128_t saved[32];memcpy(saved,s->gpr,sizeof saved);
  assert(ee_core_step_n(1)==1&&!s->halted&&!s->exc_raised_this_step);
  assert(s->fcr31==(before&~0x00800000u)&&s->fpr[2]==values[a]&&s->fpr[3]==values[d]);
  assert(!memcmp(saved,s->gpr,sizeof saved));checks++;
 }
 const uint32_t unknown[]={
  0x4c000000u,0xc8000000u,1u,(1u<<26)|(4u<<16),
  (16u<<26)|(3u<<21),(16u<<26)|(16u<<21)|3u,
  (17u<<26)|(16u<<21)|8u,(17u<<26)|(20u<<21)|1u,
  (17u<<26)|(8u<<21)|(4u<<16),(17u<<26)|(3u<<21),
  (18u<<26)|(8u<<21)|(4u<<16),(18u<<26)|(3u<<21),
  (28u<<26)|2u,(28u<<26)|(15u<<6)|8u,
  (28u<<26)|40u,(28u<<26)|(1u<<6)|9u,(28u<<26)|(1u<<6)|41u};
 for(unsigned n=0;n<sizeof unknown/sizeof *unknown;n++)for(unsigned delay=0;delay<2;delay++)for(unsigned bev=0;bev<2;bev++){
  *s=initial;s->pc=pc;s->next_pc=delay?pc+0x100u:pc+4;s->branch_pending=(uint8_t)delay;s->cop0[12]=bev?0x400000u:0;
  s->cop0[13]=0x10000100u;s->gpr[4].ud0=0x1122334455667788ull;s->gpr[4].ud1=0xdeadbeef12345678ull;
  put(s->ram+0x200000,unknown[n]);ee_reg128_t before=s->gpr[4];
  assert(ee_core_step_n(1)==1&&!s->halted&&s->exc_raised_this_step);
  assert(((s->cop0[13]>>2)&31u)==10u&&!!(s->cop0[13]&0x80000000u)==delay);
  assert(s->cop0[14]==pc-delay*4u&&(s->cop0[12]&2u));
  assert(s->pc==(bev?0xbfc00380u:0x80000180u)&&s->next_pc==s->pc+4u&&!s->branch_pending);
  assert(!memcmp(&before,s->gpr+4,sizeof before));checks++;
 }
 *s=initial;s->pc=pc;s->next_pc=pc+4;s->cop0[12]=2;s->cop0[14]=0x12340000u;s->cop0[13]=0x80000100u;
 put(s->ram+0x200000,unknown[0]);assert(ee_core_step_n(1)==1);
 assert(s->cop0[14]==0x12340000u&&(s->cop0[13]&0x80000000u)&&((s->cop0[13]>>2)&31u)==10u);
 printf("PASS %u legal C.F.S/reserved EE opcode cases plus nested EXL; full GPRs, EPC/BD and both vectors\n",checks);return 0;
}
