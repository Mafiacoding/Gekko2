#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "core/ee/ee_core.h"
#include "core/recompiler/ee_block_policy.h"
int main(void) {
 ee_state_t s;memset(&s,0,sizeof(s));uint8_t ram[32];s.ram=ram;s.ram_size=0x400000;
 s.cop0[10]=7;s.tlb[0].entry_hi=0x300007;s.tlb[0].entry_lo0=(0x300u<<6)|6;
 const unsigned ops[]={0x22,0x26,0x2a,0x2e,0x1a,0x1b,0x2c,0x2d};
 for(unsigned i=0;i<8;i++) {
  unsigned op=ops[i],width=i<4?4:8,store=(i%4)>=2;
  uint32_t iw=(op<<26)|(3u<<21)|(2u<<16)|0xfffcu;
  assert(ee_jit_block_candidate(iw)&&ee_jit_block_memory_merge(iw));
  assert(ee_jit_block_memory_width(iw)==width&&ee_jit_block_memory_store(iw)==store);
  for(unsigned k=0;k<width;k++) {
   s.gpr[3].ud0=0x80300004+k;assert(ee_core_block_memory_resolve(&s,iw)==0x300001);
   assert(ee_core_block_memory_safe(&s,iw));
   s.gpr[3].ud0=0x300004+k;assert(ee_core_block_memory_resolve(&s,iw)==0x300001);
   s.gpr[3].ud0=0x300000+4096-width+4+k;
   assert(ee_core_block_memory_resolve(&s,iw)==0x300000+4096-width+1);
  }
  s.gpr[3].ud0=0x300004;s.tlb[0].entry_lo0=(0x300u<<6)|2;
  assert(!!ee_core_block_memory_resolve(&s,iw)==!store);
  s.tlb[0].entry_lo0=(0x300u<<6)|4;assert(!ee_core_block_memory_resolve(&s,iw));
  s.tlb[0].entry_lo0=(0x300u<<6)|6;s.cop0[10]=8;assert(!ee_core_block_memory_resolve(&s,iw));s.cop0[10]=7;
  s.gpr[3].ud0=0x10000007;assert(!ee_core_block_memory_resolve(&s,iw));
  s.gpr[3].ud0=0x70000007;assert(!ee_core_block_memory_resolve(&s,iw));
  s.ram_size=16;s.gpr[3].ud0=0x80000004+16-width;assert(ee_core_block_memory_resolve(&s,iw)==16-width+1);
  s.gpr[3].ud0=0x80000014;assert(!ee_core_block_memory_resolve(&s,iw));s.ram_size=0x400000;
 }
 puts("PASS all eight merge RAM proofs: byte positions, signed EA, bounds, MMIO, V/D and ASID");return 0;
}
