#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "core/ee/ee_core.h"
static uint32_t iw(unsigned op){return (op<<26)|(3u<<21)|(2u<<16);}
int main(void)
{
 ee_state_t st;memset(&st,0,sizeof(st));uint8_t ram[64];st.ram=ram;st.ram_size=64;
 st.cop0[10]=7;st.tlb[0].entry_hi=0x300007;st.tlb[0].entry_lo0=6;st.tlb[0].entry_lo1=6;
 const unsigned ops[]={0x20,0x24,0x21,0x25,0x23,0x28,0x29,0x2b};
 for(unsigned n=0;n<8;n++) {
  st.gpr[3].ud0=0x300004;ee_state_t old=st;
  assert(ee_core_block_memory_resolve(&st,iw(ops[n]))==5);assert(!memcmp(&st,&old,sizeof(st)));
  st.gpr[3].ud0=0x301004;assert(ee_core_block_memory_resolve(&st,iw(ops[n]))==5);
  st.gpr[3].ud0=0x80000000;assert(ee_core_block_memory_resolve(&st,iw(ops[n]))==1);
 }
 st.gpr[3].ud0=0x300000;st.cop0[10]=8;assert(!ee_core_block_memory_resolve(&st,iw(0x23)));
 st.tlb[0].entry_lo0=7;st.tlb[0].entry_lo1=7;assert(ee_core_block_memory_resolve(&st,iw(0x23))==1);
 st.tlb[0].entry_lo1=6;assert(!ee_core_block_memory_resolve(&st,iw(0x23)));st.cop0[10]=7;
 st.tlb[0].entry_lo0=0;assert(!ee_core_block_memory_resolve(&st,iw(0x23)));
 st.tlb[0].entry_lo0=2;assert(ee_core_block_memory_resolve(&st,iw(0x23))==1);assert(!ee_core_block_memory_resolve(&st,iw(0x2b)));
 st.tlb[0].entry_lo0=6;st.gpr[3].ud0=0x30003c;assert(ee_core_block_memory_resolve(&st,iw(0x23))==61);
 st.gpr[3].ud0=0x30003d;assert(!ee_core_block_memory_resolve(&st,iw(0x23)));
 st.gpr[3].ud0=0x300040;assert(!ee_core_block_memory_resolve(&st,iw(0x23)));
 /* Large 16KB even/odd pair and the kernel mirror; no cached translation. */
 st.tlb[0].page_mask=0x6000;st.tlb[0].entry_hi=0xffff8007;st.gpr[3].ud0=0xffff8004;
 assert(ee_core_block_memory_resolve(&st,iw(0x23))==5);
 st.gpr[3].ud0=0xffffc004;assert(ee_core_block_memory_resolve(&st,iw(0x23))==5);
 st.tlb[0].entry_lo1=(4u<<6)|6;assert(!ee_core_block_memory_resolve(&st,iw(0x23)));
 /* Virtual MMIO and scratch must decline even with valid RAM mappings. */
 const uint32_t special[]={0x10000000,0x12000000,0x70000000,0xd0000000,0xf2000000};
 st.tlb[0].page_mask=0;st.tlb[0].entry_lo0=st.tlb[0].entry_lo1=7;
 for(unsigned n=0;n<5;n++){st.gpr[3].ud0=special[n];st.tlb[0].entry_hi=special[n]|7;assert(!ee_core_block_memory_resolve(&st,iw(0x23)));}
 assert(!ee_core_block_memory_resolve(NULL,iw(0x23)));assert(!ee_core_block_memory_resolve(&st,9u<<26));
 puts("PASS live RAM resolver: zero offset, all widths, ASID/global/V/D, 16KB even/odd, kernel mirror, bounds and virtual MMIO exclusion");return 0;
}
