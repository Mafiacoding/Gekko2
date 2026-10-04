#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "core/ee/ee_core.h"
#include "core/recompiler/ee_block_policy.h"
int main(void)
{
 ee_state_t st;memset(&st,0,sizeof(st));uint8_t ram[32];st.ram=ram;st.ram_size=0x400000;
 st.cop0[10]=7;st.tlb[0].entry_hi=0x300007;st.tlb[0].entry_lo0=(0x300u<<6)|6;st.tlb[0].entry_lo1=(0x301u<<6)|6;
 for(unsigned op=0x36;op<=0x3e;op+=8) {
  uint32_t iw=(op<<26)|(3u<<21)|(2u<<16);assert(ee_jit_block_candidate(iw));assert(ee_jit_block_memory_width(iw)==16);
  st.gpr[3].ud0=0x300004;assert(ee_core_block_memory_resolve(&st,iw)==0x300005); /* no LQ-style rounding */
  st.gpr[3].ud0=0x300ff0;assert(ee_core_block_memory_resolve(&st,iw)==0x300ff1);
  st.gpr[3].ud0=0x300ff4;assert(!ee_core_block_memory_resolve(&st,iw)); /* mapped cross-page lanes remain scalar */
  st.gpr[3].ud0=0x80300ff4;assert(ee_core_block_memory_resolve(&st,iw)==0x300ff5); /* direct contiguous RAM */
  st.gpr[3].ud0=0x300001;assert(!ee_core_block_memory_resolve(&st,iw));
  st.gpr[3].ud0=0x300000;st.tlb[0].entry_lo0=(0x300u<<6)|2;
  assert(!!ee_core_block_memory_resolve(&st,iw)==(op==0x36));st.tlb[0].entry_lo0=(0x300u<<6)|6;
 }
 assert(ee_jit_block_candidate(0x36u<<26));assert(!ee_jit_block_memory_width(0x36u<<26));
 assert(ee_jit_block_memory_store(0x3eu<<26));assert(!ee_jit_block_memory_store(0x36u<<26));
 assert(ee_jit_block_memory_width(0x27u<<26)==4);
 puts("PASS vector memory proof: unmasked EA, page crossings, V/D, VF00 no-access and LWU");return 0;
}
