#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "core/ee/ee_core.h"
#include "core/recompiler/ee_block_policy.h"
int main(void)
{
 ee_state_t st;memset(&st,0,sizeof(st));uint8_t ram[128];st.ram=ram;st.ram_size=128;
 st.cop0[10]=7;st.tlb[0].entry_hi=0x300007;st.tlb[0].entry_lo0=st.tlb[0].entry_lo1=2;
 const unsigned ops[]={0x37,0x3f,0x1e,0x1f,0x31,0x39};
 const unsigned widths[]={8,8,16,16,4,4};
 for(unsigned n=0;n<6;n++) {
  uint32_t w=(ops[n]<<26)|(3u<<21)|(2u<<16);
  assert(ee_jit_block_candidate(w));assert(ee_jit_block_memory_width(w)==widths[n]);
  assert(ee_jit_block_memory_store(w)==(n==1||n==3||n==5));
  st.gpr[3].ud0=0x300000;
  assert(!!ee_core_block_memory_resolve(&st,w)==!ee_jit_block_memory_store(w));
  st.tlb[0].entry_lo0=6;st.gpr[3].ud0=0x300070;
  assert(ee_core_block_memory_resolve(&st,w)==113);
  st.gpr[3].ud0=0x30007f;
  assert(!!ee_core_block_memory_resolve(&st,w)==(widths[n]==16));
  st.gpr[3].ud0=0x300080;assert(!ee_core_block_memory_resolve(&st,w));
  st.tlb[0].entry_lo0=2;
 }
 assert(ee_jit_block_candidate(0x1eu<<26));assert(!ee_jit_block_memory_width(0x1eu<<26));
 for(unsigned op=0;op<64;op++) {
  for(unsigned rt=0;rt<32;rt++) {
   uint32_t w=(op<<26)|(rt<<16)|8;
   int want=op==2||op==3||(op>=4&&op<=7)||(op>=20&&op<=23)||(op==0)||(op==1&&rt<=3);
   assert(ee_jit_block_terminal(w)==want);
  }
 }
 assert(ee_jit_block_terminal(9));assert(!ee_jit_block_terminal(12));
 puts("PASS wide RAM admission: LD/SD/LQ/SQ/LWC1/SWC1, load/store permissions, quad alignment, zero LQ and terminal policy");return 0;
}
