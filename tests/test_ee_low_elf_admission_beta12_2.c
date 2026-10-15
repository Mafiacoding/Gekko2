/* BETA12.2: low 1-2 MiB ELF code must have a guarded native source. */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "core/ee/ee_core.c"
int main(void)
{
    ee_state_t st; memset(&st,0,sizeof st);
    st.ram_size=0x400000u;st.ram=calloc(1,st.ram_size);
    assert(st.ram);
    st.cop0[10]=7u;
    st.tlb[0].entry_hi=0x001f2007u;
    st.tlb[0].entry_lo0=((0x001f2000u>>12)<<6)|6u;
    st.tlb[0].entry_lo1=((0x001f3000u>>12)<<6)|6u;
    st.tlb[0].page_mask=0;
    st.ram[0x001f2000u]=0x09;st.ram[0x001f2003u]=0x24;
    uint32_t word=0;
    assert(ee_core_block_peek(&st,0x001f2000u,&word)&&word==0x24000009u);
    assert(ee_core_block_peek(&st,0x801f2000u,&word)&&word==0x24000009u);
    assert(!ee_core_block_peek(&st,0x800ff000u,&word));
    st.tlb[0].entry_hi=0x002f2007u;
    assert(!ee_core_block_peek(&st,0x001f2000u,&word));
    st.tlb[0].entry_hi=0x001f2007u;
    st.tlb[0].entry_lo0=0;
    assert(!ee_core_block_peek(&st,0x001f2000u,&word));
    free(st.ram);
    puts("PASS low ELF guarded native code admission / mapping invalidation / low kernel remains scalar");
    return 0;
}
