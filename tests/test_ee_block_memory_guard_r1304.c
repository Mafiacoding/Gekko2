#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "core/ee/ee_core.h"
#include "core/recompiler/ee_block_policy.h"
int main(void)
{
 ee_state_t st;memset(&st,0,sizeof(st));uint8_t ram[32]={0};st.ram=ram;st.ram_size=sizeof(ram);
 const unsigned ops[]={0x20,0x24,0x28,0x21,0x25,0x29,0x23,0x2b};
 for(unsigned n=0;n<8;n++) {
  unsigned op=ops[n],width=ee_jit_block_memory_width(op<<26);assert(width&&ee_jit_block_candidate(op<<26));
  uint32_t iw=(op<<26)|(3u<<21)|(2u<<16);
  const uint32_t good[]={0x80000000,0xa0000000,0x80000020-width,0xa0000020-width};
  for(unsigned j=0;j<4;j++){st.gpr[3].ud0=good[j];ee_state_t before=st;assert(ee_core_block_memory_safe(&st,iw));assert(!memcmp(&st,&before,sizeof(st)));}
  const uint32_t bad[]={0x00000000,0xc0000000,0x80000020,0xbfc00000,0x9fffffff,0x10000000,0x70000000};
  for(unsigned j=0;j<7;j++){st.gpr[3].ud0=bad[j];assert(!ee_core_block_memory_safe(&st,iw));}
  if(width>1){st.gpr[3].ud0=0x80000001;assert(!ee_core_block_memory_safe(&st,iw));}
  st.gpr[3].ud0=0x80000004;assert(ee_core_block_memory_safe(&st,iw|0xfffcu));
  st.ram_size=width-1;assert(!ee_core_block_memory_safe(&st,iw));st.ram_size=32;
  st.gpr[0].ud0=0x80000000;assert(!ee_core_block_memory_safe(&st,op<<26));
  st.ram=NULL;assert(!ee_core_block_memory_safe(&st,iw));st.ram=ram;
 }
 assert(!ee_jit_block_memory_width(9u<<26));puts("PASS direct-RAM guard: byte/half/word, aliases, bounds, alignment, subtraction, r0 and no state mutation");return 0;
}
