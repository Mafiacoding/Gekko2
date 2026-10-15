/* GPL-3.0-or-later. BC2 hardware condition, annulment and exception oracles. */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "core/ee/ee_core.h"
#include "core/recompiler/optimization.h"
static void put(uint8_t *p,uint32_t w){for(unsigned i=0;i<4;i++)p[i]=w>>(8*i);}
int main(void) {
    bios_image_t bios={0};bios.data=calloc(1,BIOS_MAX_SIZE);bios.size=BIOS_MAX_SIZE;bios.loaded=1;
    assert(!ee_core_init(&bios));ee_state_t *s=ee_core_get_state();ee_state_t initial=*s;
    const uint32_t pc=0x8020f000u;
    gekko2_optimization_mask&=~(GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS)|GEKKO2_OPT_BIT(GEKKO2_OPT_EE_JIT));
    for(unsigned condition=0;condition<2;condition++)for(unsigned kind=0;kind<4;kind++) {
        *s=initial;s->pc=pc;s->next_pc=pc+4;s->cop2_ctrl[29]=condition<<8;
        uint32_t branch=0x49000003u|(kind<<16);
        put(s->ram+(pc&0x1fffffff),branch);
        put(s->ram+(pc&0x1fffffff)+4,0x24030001u);
        put(s->ram+(pc&0x1fffffff)+8,0x24040002u);
        int taken=condition==(kind&1u),annul=!taken&&(kind&2u);
        assert(ee_core_step_n(1)==1 && !s->halted);
        assert(s->pc==pc+(annul?8u:4u));
        assert(s->branch_pending==!annul && !s->gpr[3].ud0);
        assert(ee_core_step_n(1)==1 && !s->halted);
        assert(s->pc==pc+(taken?16u:annul?12u:8u));
        assert(s->gpr[3].ud0==(annul?0u:1u));
        assert(s->gpr[4].ud0==(annul?2u:0u));
    }
    *s=initial;s->pc=pc;s->next_pc=pc+4;s->cop2_ctrl[29]=1u<<8;
    s->gpr[1].ud0=0x7fffffffu;s->gpr[2].ud0=1;s->gpr[3].ud0=0x55;
    put(s->ram+(pc&0x1fffffff),0x49010003u);
    put(s->ram+(pc&0x1fffffff)+4,0x00221820u);
    assert(ee_core_step_n(2)==2);
    assert(s->cop0[14]==pc && (s->cop0[13]&0x80000000u));
    assert(((s->cop0[13]>>2)&31u)==12u && s->gpr[3].ud0==0x55);
    /* VF0 is immutable, but LQC2 must still take a missing data TLB fault. */
    *s=initial;s->pc=pc;s->next_pc=pc+4;s->gpr[1].ud0=0x00400000u;
    put(s->ram+(pc&0x1fffffff),(0x36u<<26)|(1u<<21));
    assert(ee_core_step_n(1)==1 && s->mem_tlb_miss);
    assert(s->cop0[14]==pc && ((s->cop0[13]>>2)&31u)==2u);
    assert(s->vu0_vf[0][0]==0 && s->vu0_vf[0][3]==0x3f800000u);
    /* J/JAL use the PC+4 region, including a 256 MiB boundary. */
    for(unsigned op=2;op<=3;op++) {
        *s=initial;s->pc=0x0ffffffcu;s->next_pc=0x10000000u;
        s->tlb[0].entry_hi=0x0fffe000u;
        s->tlb[0].entry_lo1=(0x200u<<6)|6u;
        put(s->ram+0x200ffcu,(op<<26)|4u);
        assert(ee_core_step_n(1)==1);
        assert(s->pc==0x10000000u && s->next_pc==0x10000010u && s->branch_pending);
        if(op==3)assert(s->gpr[31].ud0==0x10000004u);
    }
    puts("PASS BC2 condition matrix, delay slots, likely annulment, precise overflow BD/EPC");
    return 0;
}
