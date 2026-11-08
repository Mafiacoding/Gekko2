/* GPL-3.0-or-later. Batch A11: parsed PS2Recomp functions, lowered through the
 * existing Gekko2 AOT generator, against the real EE interpreter. Host only.
 * a11_fixtures.h is written by the runner from the strict parser's output. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core/recompiler/ee_aot.h"
#include "core/recompiler/ee_jit.h"
#include "core/recompiler/optimization.h"
#include "a11_fixtures.h"
static uint64_t seed=UINT64_C(0xa11a11a11);
static uint64_t random64(void){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;}
static void write32(uint8_t *p,uint32_t w){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(w>>(8*i));}
static void same(const ee_state_t *a,const ee_state_t *b)
{
    assert(!memcmp(a->gpr,b->gpr,sizeof a->gpr));
    assert(!memcmp(&a->hi,&b->hi,sizeof a->hi)&&!memcmp(&a->lo,&b->lo,sizeof a->lo));
    assert(a->pc==b->pc&&a->next_pc==b->next_pc&&a->branch_pending==b->branch_pending);
    assert(a->instructions_executed==b->instructions_executed);
    assert(!memcmp(a->cop0,b->cop0,sizeof a->cop0));
    assert(a->exc_this_pc==b->exc_this_pc&&a->exc_in_delay_slot==b->exc_in_delay_slot);
    assert(a->halted==b->halted&&a->sa_reg==b->sa_reg);
    assert(!memcmp(a->fpr,b->fpr,sizeof a->fpr)&&a->fcr31==b->fcr31&&a->acc==b->acc);
    assert(!memcmp(a->vu0_vf,b->vu0_vf,sizeof a->vu0_vf)&&!memcmp(a->tlb,b->tlb,sizeof a->tlb));
}
static void load(ee_state_t *st,const a11_fixture *f,const uint32_t *words)
{
    for(unsigned i=0;i<f->count;i++)write32(st->ram+(f->pc&0x1fffffffu)+4*i,words[i]);
    ee_jit_notify_physical_write(f->pc&0x1fffffffu,4u*f->count);
}
static void randomise(ee_state_t *st,const ee_state_t *initial,const a11_fixture *f,unsigned trial)
{
    *st=*initial;
    for(unsigned r=1;r<32;r++){st->gpr[r].ud0=random64();st->gpr[r].ud1=random64();}
    if(trial<4)for(unsigned r=1;r<32;r++)st->gpr[r].ud0=trial==0?0:trial==1?UINT64_MAX:trial==2?UINT64_C(0x8000000080000000):UINT64_C(0x7fffffff);
    st->gpr[0].ud0=0;st->gpr[0].ud1=0;
    st->hi.ud0=random64();st->hi.ud1=random64();st->lo.ud0=random64();st->lo.ud1=random64();st->sa_reg=(uint32_t)random64()&31u;
    st->pc=f->pc;st->next_pc=f->pc+4;st->branch_pending=0;
}
int main(void)
{
    bios_image_t bios={0};
    bios.data=calloc(1,BIOS_MAX_SIZE);bios.size=BIOS_MAX_SIZE;bios.loaded=1;assert(bios.data);
    assert(!ee_core_init(&bios));
    ee_state_t *st=ee_core_get_state();
    st->tlb[0].entry_hi=0x00100000u;
    st->tlb[0].entry_lo0=(0x100u<<6)|6u;
    st->tlb[0].entry_lo1=(0x101u<<6)|6u;
    ee_state_t initial=*st,scalar,start;
    assert(ee_aot_block_count()==a11_fixture_count);
    unsigned checked=0,partial=0,guarded=0;
    for(unsigned fi=0;fi<a11_fixture_count;fi++) {
        const a11_fixture *f=&a11_fixtures[fi];
        const unsigned budgets[]={f->count,1u,2u,3u,f->count-1u};
        for(unsigned trial=0;trial<24;trial++) for(unsigned bi=0;bi<5;bi++) {
            unsigned budget=budgets[bi];
            *st=initial;load(st,f,f->words);randomise(st,&initial,f,trial);start=*st;
            gekko2_optimization_mask&=~GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS);
            unsigned scalar_retired=0;
            while(scalar_retired<f->count){unsigned n=f->count-scalar_retired<budget?f->count-scalar_retired:budget;
                unsigned got=ee_core_step_n(n);assert(got==n&&!st->halted);scalar_retired+=got;}
            scalar=*st;
            *st=start;
            gekko2_optimization_mask|=GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS)|GEKKO2_OPT_BIT(GEKKO2_OPT_EE_JIT);
            uint64_t native=ee_aot_retired_count();
            unsigned aot_retired=0,first=0;
            while(aot_retired<f->count){unsigned n=f->count-aot_retired<budget?f->count-aot_retired:budget;
                unsigned got=ee_core_step_n(n);
                if(!aot_retired)first=got;
                if(got!=n){fprintf(stderr,"retirement mismatch fn=%u trial=%u budget=%u want=%u got=%u\n",fi,trial,budget,n,got);return 1;}
                aot_retired+=got;}
            /* Generated entry ran at the verified start PC and retired exactly once. */
            assert(ee_aot_retired_count()-native>=first&&first>0);
            if(budget==f->count)assert(ee_aot_retired_count()-native==f->count);
            else partial++;
            same(&scalar,st);
            assert(st->pc==f->pc+4u*f->count);
            assert(st->instructions_executed==start.instructions_executed+f->count);
            /* GPR0 immutable; every upper 64-bit lane preserved; low lane only
             * changed in registers the function can write. */
            assert(st->gpr[0].ud0==0&&st->gpr[0].ud1==0);
            for(unsigned r=0;r<32;r++){assert(st->gpr[r].ud1==start.gpr[r].ud1);
                if(!(f->dest_mask>>r&1u))assert(st->gpr[r].ud0==start.gpr[r].ud0);}
            assert(!memcmp(st->vu0_vf,start.vu0_vf,sizeof st->vu0_vf)&&st->hi.ud1==start.hi.ud1&&st->lo.ud1==start.lo.ud1);
            checked++;
        }
    }
    /* Live-source guards: a changed guest word must never run stale generated
     * code. 0 = first word, 1 = last word, 2 = a middle word is rewritten. */
    for(unsigned fi=0;fi<a11_fixture_count;fi++) for(unsigned where=0;where<3;where++) {
        const a11_fixture *f=&a11_fixtures[fi];
        uint32_t m[32];memcpy(m,f->words,sizeof m);
        unsigned index=where==0?0u:where==1?f->count-1u:f->count/2u;
        m[index]=0x24090007u; /* addiu t1,zero,7 */
        *st=initial;load(st,f,m);randomise(st,&initial,f,5);start=*st;
        gekko2_optimization_mask&=~GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS);
        assert(ee_core_step_n(f->count)==f->count);scalar=*st;
        *st=start;gekko2_optimization_mask|=GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS)|GEKKO2_OPT_BIT(GEKKO2_OPT_EE_JIT);
        uint64_t native=ee_aot_retired_count();
        assert(ee_core_step_n(f->count)==f->count);
        /* Native code stops before the changed word; the interpreter finishes. */
        if(m[0]!=f->words[0])assert(ee_aot_retired_count()==native);
        else assert(ee_aot_retired_count()-native<=index);
        same(&scalar,st);guarded++;
    }
    /* First-word guard of the AOT entry itself. */
    {const a11_fixture *f=&a11_fixtures[0];*st=initial;load(st,f,f->words);randomise(st,&initial,f,6);
     ee_state_t before=*st;assert(!ee_aot_try_execute_fetched(st,f->count,f->words[0]^1u));same(&before,st);}
    printf("PASS PS2Recomp parsed-function differential: %u parsed-function/interpreter comparisons (%u partial-budget), %u functions, 128-bit GPR upper lanes, GPR0, retirement and %u live-source guard cases\n",checked,partial,a11_fixture_count,guarded);
    return 0;
}
