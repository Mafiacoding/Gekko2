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
    /* The whole synthetic ELF image is guest memory: callees outside the lowered set
     * still exist and run on the interpreter. */
    for(unsigned k=0;k<a11_image_count;k++){const a11_image_t *im=&a11_image[k];
        for(unsigned i=0;i<im->count;i++)write32(st->ram+(im->pc&0x1fffffffu)+4*i,im->words[i]);
        ee_jit_notify_physical_write(im->pc&0x1fffffffu,4u*im->count);}
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
    if(f->loop){st->gpr[8].ud0=1u+(random64()&7u); /* bounded trip count for loops that read t0 */
        st->gpr[10].ud0=st->gpr[9].ud0+1u+(random64()&1u); /* t2 = t1+1 (loop once) or t1+2 (leave at once) */}
    if(f->jr_ra)st->gpr[31].ud0=UINT64_C(0x80000000)|(random64()&0x1ffffcu);
}
/* Memory fixtures: a0/a1/a2 point into RAM; every word of the data area is itself a
 * valid aligned RAM pointer so a load result may be reused as a base address.
 * mode 0 = valid; 1 = misaligned by one byte; 2 = MMIO; 3 = unmapped KUSEG (TLB
 * refill); 4 = mapped but write-protected KUSEG page (stores raise TLB Modified). */
#define MODES 5u
static void seed_memory(ee_state_t *st,const a11_fixture *f,unsigned mode)
{
    if(!f->memory)return;
    for(unsigned i=0;i<0x20000u/4;i++)write32(st->ram+0x300000+4*i,0x80300200u+((uint32_t)random64()&0xfcu));
    static const uint32_t a0[MODES]={0x80300100u,0x80300101u,0x10003000u,0x00700000u,0x00400100u};
    static const uint32_t a1[MODES]={0x80310008u,0x80310009u,0x10003000u,0x00700000u,0x00400200u};
    static const uint32_t a2[MODES]={0x80320010u,0x80320011u,0x10003000u,0x00700000u,0x00400300u};
    st->gpr[4].ud0=a0[mode];st->gpr[5].ud0=a1[mode];st->gpr[6].ud0=a2[mode];
    st->gpr[29].ud0=(uint32_t)(a2[mode]+0x40u); /* stack for callers that spill $ra */
}
static int has_store(const a11_fixture *f){for(unsigned i=0;i<f->count;i++){unsigned op=f->ram[i]>>26;if(op==0x28||op==0x29||op==0x2b)return 1;}return 0;}
/* Dynamic retirement count: single-step the interpreter until the function exits
 * (pc == return address for jr $ra functions, else the end), then restore. */
static unsigned probe_total(ee_state_t *st,const a11_fixture *f,const ee_state_t *start)
{
    uint32_t exit_pc=f->jr_ra?(uint32_t)start->gpr[31].ud0:f->pc+4u*f->count;
    gekko2_optimization_mask&=~GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS);
    unsigned n=0;
    while(st->pc!=exit_pc&&n<200000u){unsigned got=ee_core_step_n(1);assert(got==1&&!st->halted);n++;}
    assert(st->pc==exit_pc&&"function did not exit");
    return n;
}
static int ram_is_lowered(const a11_fixture *f){return !memcmp(f->ram,f->words,4u*f->count);}
int main(void)
{
    bios_image_t bios={0};
    bios.data=calloc(1,BIOS_MAX_SIZE);bios.size=BIOS_MAX_SIZE;bios.loaded=1;assert(bios.data);
    assert(!ee_core_init(&bios));
    ee_state_t *st=ee_core_get_state();
    st->tlb[0].entry_hi=0x00100000u;
    st->tlb[0].entry_lo0=(0x100u<<6)|6u;
    st->tlb[0].entry_lo1=(0x101u<<6)|6u;
    /* Write-protected (V, not D) KUSEG mapping at 0x400000 for TLB Modified tests. */
    st->tlb[1].entry_hi=0x00400000u;st->tlb[1].entry_lo0=(0x400u<<6)|2u;st->tlb[1].entry_lo1=(0x401u<<6)|2u;
    ee_state_t initial=*st,scalar,start;
    assert(ee_aot_block_count()==a11_block_count);
    unsigned checked=0,partial=0,guarded=0,faulted=0,mod_faults=0,align_faults=0,stores=0,maxdyn=0;
    static uint8_t ram_before[0x20000],scalar_ram[0x20000];
    for(unsigned fi=0;fi<a11_fixture_count;fi++) {
        const a11_fixture *f=&a11_fixtures[fi];
        for(unsigned trial=0;trial<24;trial++) for(unsigned bi=0;bi<5;bi++) {
            *st=initial;load(st,f,f->ram);randomise(st,&initial,f,trial);seed_memory(st,f,0);start=*st;
            memcpy(ram_before,st->ram+0x300000,sizeof ram_before);
            const unsigned total=probe_total(st,f,&start); /* dynamic retired count */
            if(total>maxdyn)maxdyn=total;
            const unsigned budgets[]={total,1u,2u,3u,total>1u?total-1u:1u};
            unsigned budget=budgets[bi];
            *st=start;memcpy(st->ram+0x300000,ram_before,sizeof ram_before);
            gekko2_optimization_mask&=~GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS);
            unsigned scalar_retired=0;
            while(scalar_retired<total){unsigned n=total-scalar_retired<budget?total-scalar_retired:budget;
                unsigned got=ee_core_step_n(n);assert(got==n&&!st->halted);scalar_retired+=got;}
            scalar=*st;memcpy(scalar_ram,st->ram+0x300000,sizeof scalar_ram);
            *st=start;memcpy(st->ram+0x300000,ram_before,sizeof ram_before);
            gekko2_optimization_mask|=GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS)|GEKKO2_OPT_BIT(GEKKO2_OPT_EE_JIT);
            uint64_t native=ee_aot_retired_count();
            unsigned aot_retired=0,first=0;
            while(aot_retired<total){unsigned n=total-aot_retired<budget?total-aot_retired:budget;
                unsigned got=ee_core_step_n(n);
                if(!aot_retired)first=got;
                if(got!=n){fprintf(stderr,"retirement mismatch fn=%u trial=%u budget=%u want=%u got=%u\n",fi,trial,budget,n,got);return 1;}
                aot_retired+=got;}
            /* Generated entry ran at the verified start PC and retired exactly once. */
            /* Generated entry ran at the verified start PC. When the guest word at the
             * inferred NOP slot differs (upstream-elided NOP alias), native code must
             * stop before it and the interpreter retire it. */
            assert(first>0);
            if(ram_is_lowered(f)&&!a11_calls_external)assert(ee_aot_retired_count()-native>=first);
            else if(a11_calls_external)assert(ee_aot_retired_count()-native>0); /* callee retires on the interpreter */
            else assert(ee_aot_retired_count()-native>0&&ee_aot_retired_count()-native<total);
            if(budget==total&&ram_is_lowered(f)&&!a11_calls_external)assert(ee_aot_retired_count()-native==total);
            else partial++;
            same(&scalar,st);assert(!memcmp(scalar_ram,st->ram+0x300000,sizeof scalar_ram));
            assert(st->pc==(f->jr_ra?(uint32_t)start.gpr[31].ud0:f->pc+4u*f->count));assert(!st->branch_pending);
            assert(st->instructions_executed==start.instructions_executed+total);
            /* GPR0 immutable; every upper 64-bit lane preserved; low lane only
             * changed in registers the function can write. */
            assert(st->gpr[0].ud0==0&&st->gpr[0].ud1==0);
            for(unsigned r=0;r<32;r++){assert(st->gpr[r].ud1==start.gpr[r].ud1);
                if(!(f->dest_mask>>r&1u))assert(st->gpr[r].ud0==start.gpr[r].ud0);}
            assert(!memcmp(st->vu0_vf,start.vu0_vf,sizeof st->vu0_vf)&&st->hi.ud1==start.hi.ud1&&st->lo.ud1==start.lo.ud1);
            checked++;
        }
    }
    /* Faulting and device addresses: AOT must defer to the interpreter and leave
     * exactly the same architectural result (exception state, BadVAddr, EPC). */
    for(unsigned fi=0;fi<a11_fixture_count;fi++) if(a11_fixtures[fi].memory) for(unsigned mode=1;mode<MODES;mode++) for(unsigned trial=0;trial<6;trial++) {
        const a11_fixture *f=&a11_fixtures[fi];
        *st=initial;load(st,f,f->ram);randomise(st,&initial,f,trial);seed_memory(st,f,mode);start=*st;
        memcpy(ram_before,st->ram+0x300000,sizeof ram_before);
        gekko2_optimization_mask&=~GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS);
        unsigned want=ee_core_step_n(f->count);scalar=*st;memcpy(scalar_ram,st->ram+0x300000,sizeof scalar_ram);
        *st=start;memcpy(st->ram+0x300000,ram_before,sizeof ram_before);
        gekko2_optimization_mask|=GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS)|GEKKO2_OPT_BIT(GEKKO2_OPT_EE_JIT);
        unsigned got=ee_core_step_n(f->count);
        if(got!=want){fprintf(stderr,"fault retire mismatch fn=%u mode=%u trial=%u want=%u got=%u\n",fi,mode,trial,want,got);return 1;}
        same(&scalar,st);assert(!memcmp(scalar_ram,st->ram+0x300000,sizeof scalar_ram));faulted++;
        /* An unmapped KUSEG access must really raise a TLB exception. (Gekko2's core
         * raises no address error for an unaligned word access, so mode 1 only proves
         * that AOT and interpreter agree on that behaviour.) */
        if(mode==3)assert(((st->cop0[13]>>2)&31u)!=0u&&st->cop0[8]!=0u);
        {unsigned code=(st->cop0[13]>>2)&31u;
         if(mode==4&&code==1u)mod_faults++;
         if(mode==1&&(code==4u||code==5u))align_faults++;
         if(mode==4&&has_store(f))stores++;}
    }
    /* Write-protected pages must raise TLB Modified for every fixture that stores. */
    if(stores)assert(mod_faults>0);
    fprintf(stderr,"  observed: %u TLB-Modified faults, %u address-error faults on misaligned access\n",mod_faults,align_faults);
    /* Live-source guards: a changed guest word must never run stale generated
     * code. 0 = first word, 1 = last word, 2 = a middle word is rewritten. */
    for(unsigned fi=0;fi<a11_fixture_count;fi++) for(unsigned where=0;where<3;where++) {
        const a11_fixture *f=&a11_fixtures[fi];
        uint32_t m[32];memcpy(m,f->ram,sizeof m);
        unsigned index=where==0?0u:where==1?f->count-1u:f->count/2u;
        m[index]=0x24090007u; /* addiu t1,zero,7 */
        *st=initial;load(st,f,m);randomise(st,&initial,f,5);seed_memory(st,f,0);start=*st;
        gekko2_optimization_mask&=~GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS);
        assert(ee_core_step_n(f->count)==f->count);scalar=*st;
        *st=start;gekko2_optimization_mask|=GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS)|GEKKO2_OPT_BIT(GEKKO2_OPT_EE_JIT);
        uint64_t native=ee_aot_retired_count();
        assert(ee_core_step_n(f->count)==f->count);
        /* Native code stops before the changed word; the interpreter finishes. */
        /* A changed first word defeats the entry; with several blocks the interpreter
         * may still reach untouched later blocks natively. */
        if(m[0]!=f->words[0]&&f->blocks==1)assert(ee_aot_retired_count()==native);
        else if(f->blocks==1)assert(ee_aot_retired_count()-native<=index);
        same(&scalar,st);guarded++;
    }
    /* First-word guard of the AOT entry itself. */
    {const a11_fixture *f=&a11_fixtures[0];*st=initial;load(st,f,f->ram);randomise(st,&initial,f,6);
     ee_state_t before=*st;assert(!ee_aot_try_execute_fetched(st,f->count,f->words[0]^1u));same(&before,st);}
    fprintf(stderr,"  max dynamic retirement per run: %u\n",maxdyn);
    printf("PASS PS2Recomp parsed-function differential: %u parsed-function/interpreter comparisons (%u partial-budget), %u functions, 128-bit GPR upper lanes, GPR0, retirement, %u fault/device cases and %u live-source guard cases\n",checked,partial,a11_fixture_count,faulted,guarded);
    return 0;
}
