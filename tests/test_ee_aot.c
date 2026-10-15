/* GPL-3.0-or-later. Differential execution against the real EE interpreter. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core/recompiler/ee_aot.h"
#include "core/recompiler/optimization.h"
#include "aot_test_fixture.h"
static uint64_t seed=UINT64_C(0x4732b19a);
static uint64_t random64(void){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;}
static void write32(uint8_t *p,uint32_t w){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(w>>(8*i));}
static void same(const ee_state_t *a,const ee_state_t *b)
{
    assert(!memcmp(a->gpr,b->gpr,sizeof a->gpr));
    assert(!memcmp(&a->hi,&b->hi,sizeof a->hi));
    assert(!memcmp(&a->lo,&b->lo,sizeof a->lo));
    assert(a->pc==b->pc&&a->next_pc==b->next_pc);
    assert(a->branch_pending==b->branch_pending);
    assert(a->instructions_executed==b->instructions_executed);
    assert(!memcmp(a->cop0,b->cop0,sizeof a->cop0));
    assert(a->exc_this_pc==b->exc_this_pc&&a->exc_in_delay_slot==b->exc_in_delay_slot);
    assert(a->mem_tlb_miss==b->mem_tlb_miss&&a->halted==b->halted);
    assert(a->sa_reg==b->sa_reg);
    assert(!memcmp(a->fpr,b->fpr,sizeof a->fpr));assert(a->fcr31==b->fcr31&&a->acc==b->acc);
    assert(!memcmp(a->cop2_ctrl,b->cop2_ctrl,sizeof a->cop2_ctrl));
    assert(!memcmp(a->vu0_vf,b->vu0_vf,sizeof a->vu0_vf));
    assert(!memcmp(a->vu0_mem,b->vu0_mem,sizeof a->vu0_mem));
    assert(!memcmp(a->tlb,b->tlb,sizeof a->tlb));
}
int main(void)
{
    bios_image_t bios={0};
    bios.data=calloc(1,BIOS_MAX_SIZE);bios.size=BIOS_MAX_SIZE;bios.loaded=1;assert(bios.data);
    assert(!ee_core_init(&bios));
    ee_state_t *st=ee_core_get_state();
    st->tlb[0].entry_hi=0x00200000u;
    st->tlb[0].entry_lo0=(0x200u<<6)|6u;
    st->tlb[0].entry_lo1=(0x201u<<6)|6u;
    ee_state_t initial=*st, scalar;
    uint8_t data[128], scalar_data[128];
    assert(ee_aot_block_count()==aot_fixture_count);
    unsigned checked=0;
    for(unsigned f=0;f<aot_fixture_count;f++) {
        const aot_fixture *entry=&aot_fixtures[f];
        for(unsigned i=0;i<entry->count;i++)write32(st->ram+(entry->pc&0x1fffffffu)+4*i,entry->words[i]);
        write32(st->ram+(entry->pc&0x1fffffffu)+4*entry->count,0u);
        for(unsigned trial=0;trial<12;trial++) {
            *st=initial;
            for(unsigned r=1;r<32;r++){st->gpr[r].ud0=random64();st->gpr[r].ud1=random64();}
            for(unsigned r=0;r<32;r++){st->fpr[r]=(uint32_t)random64();for(unsigned l=0;l<4;l++)if(r)st->vu0_vf[r][l]=(uint32_t)random64();}
            /* Exercise zero/negative comparisons and high halves as well. */
            if(trial<4){st->gpr[1].ud0=trial==0?0:trial==1?UINT64_MAX:trial==2?UINT64_C(0x8000000000000000):UINT64_C(0x7fffffff);st->gpr[2].ud0=st->gpr[1].ud0;}
            if(entry->memory)st->gpr[1].ud0=0x80300000u;
            st->gpr[4].ud0=0x80300000u;
            st->hi.ud0=random64();st->lo.ud0=random64();
            st->pc=entry->pc;st->next_pc=entry->pc+4;
            for(unsigned i=0;i<sizeof data;i++)data[i]=(uint8_t)random64();
            memcpy(st->ram+0x300000,data,sizeof data);
            ee_state_t start=*st;
            unsigned budget=trial%2?(entry->count==1?2:1):entry->count;
            gekko2_optimization_mask &= ~GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS);
            unsigned scalar_retired=ee_core_step_n(budget);scalar=*st;
            if(st->halted){fprintf(stderr,"scalar halted fixture=%u word=%08x trial=%u: %s\n",f,entry->words[0],trial,st->halt_reason);return 1;}
            memcpy(scalar_data,st->ram+0x300000,sizeof scalar_data);
            *st=start;memcpy(st->ram+0x300000,data,sizeof data);
            uint64_t retired=ee_aot_retired_count();
            gekko2_optimization_mask |= GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS)|GEKKO2_OPT_BIT(GEKKO2_OPT_EE_JIT);
            unsigned aot_retired=ee_core_step_n(budget);
            if(aot_retired!=scalar_retired){fprintf(stderr,"retirement mismatch fixture=%u word=%08x trial=%u scalar=%u aot=%u\n",f,entry->words[0],trial,scalar_retired,aot_retired);return 1;}
            assert(ee_aot_retired_count()>retired);
            same(&scalar,st);assert(!memcmp(scalar_data,st->ram+0x300000,sizeof scalar_data));
            checked++;
        }
    }
    const aot_fixture *entry=&aot_fixtures[0];
    *st=initial;st->pc=entry->pc;st->next_pc=entry->pc+4;
    ee_state_t before=*st;
    assert(!ee_aot_try_execute_fetched(st,2,entry->words[0]^1));same(&before,st);
    write32(st->ram+(entry->pc&0x1fffffffu),entry->words[0]^1);
    assert(!ee_aot_try_execute_fetched(st,2,entry->words[0]));same(&before,st);
    write32(st->ram+(entry->pc&0x1fffffffu),entry->words[0]);
    write32(st->ram+(entry->pc&0x1fffffffu)+4,0x24030001u);
    assert(ee_aot_try_execute_fetched(st,2,entry->words[0])==1);
    assert(st->pc==entry->pc+4&&st->instructions_executed==before.instructions_executed+1);
    /* A selected straight-line entry may be an actual delay slot. It
     * retires exactly once and exposes the older branch target. */
    *st=before;st->branch_pending=1;st->next_pc=entry->pc+64u;
    ee_state_t delay_start=*st;
    gekko2_optimization_mask &= ~GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS);
    assert(ee_core_step_n(1)==1);scalar=*st;
    *st=delay_start;
    assert(ee_aot_try_execute_fetched(st,2,entry->words[0])==1);
    same(&scalar,st);
    gekko2_optimization_mask |= GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS);
    *st=before;assert(!ee_aot_try_execute_fetched(st,0,entry->words[0]));
    *st=before;ee_core_interleave_quantum=1u;
    assert(ee_aot_try_execute_fetched(st,32u,entry->words[0])==1u);
    ee_core_interleave_quantum=UINT32_MAX;
    /* A Count/Compare IRQ at the first retirement must exit the block. */
    *st=before;st->cop0[12]=0x00418001u;st->cop0[11]=st->cop0[9]+1u;
    assert(ee_aot_try_execute_fetched(st,2u,entry->words[0])==1u);
    assert(st->cop0[12]&2u);assert(st->cop0[14]==entry->pc+4u);
    assert(!(st->cop0[13]&0x80000000u));
    /* A changed KUSEG mapping is checked through the current TLB. */
    entry=&aot_fixtures[aot_fixture_count-1];
    assert(entry->pc==0x00200000u);
    *st=initial;st->pc=entry->pc;st->next_pc=entry->pc+4u;
    st->tlb[0].entry_lo0=(0x300u<<6)|6u;
    write32(st->ram+0x300000,0x0000000cu);before=*st;
    assert(!ee_aot_try_execute_fetched(st,2u,entry->words[0]));same(&before,st);
    /* Guard an otherwise compiled memory block whose address is MMIO. */
    for(unsigned f=0;f<aot_fixture_count;f++)if(aot_fixtures[f].memory) {
        entry=&aot_fixtures[f];*st=initial;st->pc=entry->pc;st->next_pc=entry->pc+4;
        st->gpr[1].ud0=0x10003000u;before=*st;
        assert(!ee_aot_try_execute_fetched(st,2,entry->words[0]));same(&before,st);break;
    }
    /* A fused slot is fetched again after the branch retires. */
    for(unsigned f=0;f<aot_fixture_count;f++)if(aot_fixtures[f].words[0]==0x10220003u &&
                                               aot_fixtures[f].words[1]==0x24630001u) {
        entry=&aot_fixtures[f];*st=initial;
        st->pc=entry->pc;st->next_pc=entry->pc+4;
        write32(st->ram+(entry->pc&0x1fffffffu),entry->words[0]);
        write32(st->ram+(entry->pc&0x1fffffffu)+4,0x24630002u);
        assert(ee_aot_try_execute_fetched(st,2,entry->words[0])==1);
        assert(st->branch_pending && st->pc==entry->pc+4 && !st->gpr[3].ud0);
        assert(ee_core_step_n(1)==1 && st->gpr[3].ud0==2u);
        write32(st->ram+(entry->pc&0x1fffffffu)+4,entry->words[1]);
        *st=initial;st->pc=entry->pc;st->next_pc=entry->pc+4;
        st->cop0[12]=0x00418001u;st->cop0[11]=st->cop0[9]+1u;
        ee_state_t irq_start=*st;
        gekko2_optimization_mask&=~GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS);
        /* Count latches at the branch; the existing core takes its IRQ
         * after the slot. Fusing cannot change that boundary. */
        assert(ee_core_step_n(2)==2);scalar=*st;
        *st=irq_start;assert(ee_aot_try_execute_fetched(st,2,entry->words[0])==2);
        same(&scalar,st);gekko2_optimization_mask|=GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS);
        break;
    }
    printf("PASS EE AOT: %u interpreter comparisons; source, partial exit, delay-slot execution, quantum, precise IRQ, TLB and MMIO guards\n",checked);
    return 0;
}
