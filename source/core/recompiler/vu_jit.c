#include "core/recompiler/vu_jit.h"
#include "core/recompiler/ppc_dynarec.h"
#include "hw/vu_opcodes.h"
#include <stdlib.h>
#include <string.h>
static uint64_t upper_count,lower_count,rejected_hits,pair_count,block_count;
static uint32_t owned_count;
#if defined(GEKKO) && !defined(PCSX2WII_JIT_DISABLE)
#define VU_CODE_SLOTS 512u
#define VU_LOOKUP_SLOTS 1024u
typedef void (*vu_fn)(uint32_t (*)[4],uint32_t *,void *,uint32_t,uint32_t,uint32_t *,uint32_t *);
typedef struct {uint32_t word;vu_fn fn;uint8_t lower;} owned_entry;
typedef struct {uint32_t word;vu_fn fn;uint8_t valid,lower,rejected;} lookup_entry;
typedef void (*pair_fn)(uint32_t (*)[4],uint32_t *,uint32_t *,uint8_t *,uint32_t,uint32_t,uint32_t *,uint32_t *);
#define VU_PAIR_SLOTS 128u
typedef struct { uint32_t upper,lower;pair_fn fn;uint8_t valid,rejected; } pair_entry;
static pair_entry pairs[VU_PAIR_SLOTS];
#define VU_BLOCK_SLOTS 32u
typedef struct {const uint8_t *micro;uint32_t pc,mask,upper[8],lower[8];pair_fn fn;uint8_t count,valid;} block_entry;
static block_entry blocks[VU_BLOCK_SLOTS];
static owned_entry owned[VU_CODE_SLOTS];
static lookup_entry lookup[VU_LOOKUP_SLOTS];
static unsigned hash(uint32_t word,unsigned lower,unsigned mask) {
    return (word^(word>>10)^(word>>20)^(lower<<8))&mask;
}
static vu_fn find(uint32_t word,unsigned lower) {
    unsigned h=hash(word,lower,VU_CODE_SLOTS-1u);
    for(unsigned n=0;n<VU_CODE_SLOTS;n++) {
        owned_entry *p=&owned[(h+n)&(VU_CODE_SLOTS-1u)];
        if(!p->fn)return NULL;
        if(p->word==word && p->lower==lower)return p->fn;
    }
    return NULL;
}
static int insert(uint32_t word,unsigned lower,vu_fn fn) {
    unsigned h=hash(word,lower,VU_CODE_SLOTS-1u);
    for(unsigned n=0;n<VU_CODE_SLOTS;n++) {
        owned_entry *p=&owned[(h+n)&(VU_CODE_SLOTS-1u)];
        if(!p->fn){p->word=word;p->lower=(uint8_t)lower;p->fn=fn;owned_count++;return 1;}
    }
    return 0;
}
static vu_fn resolve(uint32_t word,unsigned lower) {
    lookup_entry *p=&lookup[hash(word,lower,VU_LOOKUP_SLOTS-1u)];
    vu_fn fn=find(word,lower);int rejected=0;
    if(!fn) {
        if(owned_count==VU_CODE_SLOTS)rejected=1;
        else {
            ppc_codegen_ctx_t c;if(ppc_dynarec_init(&c,2))return NULL;
            int result=lower?ppc_dynarec_translate_vu_lower(&c,word):ppc_dynarec_translate_vu_upper(&c,word);
            if(result){ppc_dynarec_free(&c);if(result==-2)return NULL;rejected=1;}
            else {
                fn=(vu_fn)ppc_dynarec_finalize(&c);
                if(!fn){ppc_dynarec_free(&c);return NULL;}
                if(!insert(word,lower,fn)){ppc_dynarec_free(&c);return NULL;}
            }
        }
    }
    p->word=word;p->lower=(uint8_t)lower;p->valid=1;p->fn=fn;p->rejected=(uint8_t)rejected;
    return fn;
}
static inline vu_fn hot_lookup(uint32_t word,unsigned lower) {
    lookup_entry *p=&lookup[hash(word,lower,VU_LOOKUP_SLOTS-1u)];
    if(p->valid && p->word==word && p->lower==lower) {
        if(p->rejected)rejected_hits++;
        return p->fn;
    }
    return resolve(word,lower);
}

/* Scheduler-visible lower operations are hard native-block boundaries.
 * Pair/single execution may still JIT ordinary work, but a multi-pair block
 * must return to vu_micro_step_pipeline() before Q/P publication, WAITQ/P,
 * VIF TOP/ITOP reads, PATH1 XGKICK, MFP/P reads or REG_R LFSR mutation.
 * Keeping the lower word as data when the upper I bit is set is intentional. */
static int vu_lower_pipeline_boundary(uint32_t word)
{
    if (VU_L_OPCODE(word) != VU_L_SPECIAL_OPCODE || VU_L_FUNCT6(word) < 0x3cu)
        return 0;
    unsigned fd = (word >> 6) & 31u;
    return fd == VULS_FD_DIVQ_GROUP || fd == VULS_FD_R_GROUP || fd >= 0x19u;
}

#endif
int vu_jit_try_upper(uint32_t vf[32][4],uint32_t *vi,uint32_t acc[4],uint32_t word) {
#if defined(GEKKO) && !defined(PCSX2WII_JIT_DISABLE)
    typedef void (*upper_fn)(uint32_t (*)[4],uint32_t *,uint32_t *);
    vu_fn fn=hot_lookup(word&0x01ffffffu,0);if(!fn)return 0;
    ((upper_fn)fn)(vf,vi,acc);upper_count++;return 1;
#else
    (void)vf;(void)vi;(void)acc;(void)word;return 0;
#endif
}
int vu_jit_try_lower(uint32_t vf[32][4],uint32_t *vi,uint8_t *mem,uint32_t mem_mask,uint32_t word,
                     uint32_t pc,uint32_t *branch_delay,uint32_t *branch_target) {
#if defined(GEKKO) && !defined(PCSX2WII_JIT_DISABLE)
    vu_fn fn=hot_lookup(word,1);if(!fn)return 0;
    fn(vf,vi,mem,mem_mask,pc,branch_delay,branch_target);lower_count++;return 1;
#else
    (void)vf;(void)vi;(void)mem;(void)mem_mask;(void)word;
    (void)pc;(void)branch_delay;(void)branch_target;return 0;
#endif
}
int vu_jit_try_pair(uint32_t vf[32][4],uint32_t *vi,uint32_t *acc,uint8_t *mem,
                    uint32_t mask,uint32_t pc,uint32_t *delay,uint32_t *target,
                    uint32_t upper,uint32_t lower) {
#if defined(GEKKO) && !defined(PCSX2WII_JIT_DISABLE)
    upper&=0x81ffffffu; /* I affects semantics; E/D/T remain in scheduler. */
    pair_entry *p=&pairs[(upper^(upper>>11)^lower^(lower>>17))&(VU_PAIR_SLOTS-1u)];
    if(!p->valid || p->upper!=upper || p->lower!=lower) {
        ppc_codegen_ctx_t c;if(ppc_dynarec_init(&c,4))return 0;
        int result=ppc_dynarec_translate_vu_pair(&c,upper,lower);
        if(result==-2){ppc_dynarec_free(&c);return 0;}
        pair_fn fn=NULL;
        if(!result){fn=(pair_fn)ppc_dynarec_finalize(&c);if(!fn){ppc_dynarec_free(&c);return 0;}}
        else ppc_dynarec_free(&c);
        if(p->fn)free((void*)p->fn);
        p->upper=upper;p->lower=lower;p->fn=fn;p->valid=1;p->rejected=(uint8_t)(result!=0);
    }
    if(p->rejected){rejected_hits++;return 0;}
    p->fn(vf,vi,acc,mem,mask,pc,delay,target);
    pair_count++;upper_count++;if(!(upper&0x80000000u))lower_count++;return 1;
#else
    (void)vf;(void)vi;(void)acc;(void)mem;(void)mask;(void)pc;
    (void)delay;(void)target;(void)upper;(void)lower;return 0;
#endif
}
unsigned vu_jit_try_block(uint32_t vf[32][4],uint32_t *vi,uint32_t *acc,uint8_t *mem,
                    uint32_t mask,uint8_t *micro,uint32_t micro_mask,uint32_t *pc,
                    uint32_t *delay,uint32_t *target,uint32_t *ebit,
                    uint64_t *retired,unsigned budget) {
#if defined(GEKKO) && !defined(PCSX2WII_JIT_DISABLE)
    if(*delay||*ebit||budget<2u)return 0;
    /* Local writes must not mutate later micro words inside this block. */
    uintptr_t data=(uintptr_t)mem,code=(uintptr_t)micro;
    if(data<=code+micro_mask && code<=data+mask)return 0;
    uint32_t off=*pc&micro_mask,up[8],lo[8];unsigned count=0;
    block_entry *p=&blocks[((off>>6)^(off>>11)^((uintptr_t)micro>>5))&(VU_BLOCK_SLOTS-1u)];
    /* R1283: validate exactly the cached prefix on a warm hit. Looking
     * beyond it cannot change the semantics of this bounded block and
     * wastes reads when the original compilation stopped after 2 pairs.
     * Exact full words, owner, mask, budget and entry delay guards remain. */
    int cached=p->valid&&p->micro==micro&&p->pc==off&&p->mask==micro_mask&&
        p->count>=2u&&p->count<=budget;
    for(unsigned n=0;cached&&n<p->count;n++) {
        const uint8_t *q=micro+((off+n*8u)&micro_mask);
        uint32_t low=(uint32_t)q[0]|((uint32_t)q[1]<<8)|((uint32_t)q[2]<<16)|((uint32_t)q[3]<<24);
        uint32_t high=(uint32_t)q[4]|((uint32_t)q[5]<<8)|((uint32_t)q[6]<<16)|((uint32_t)q[7]<<24);
        if(low!=p->lower[n]||high!=p->upper[n])cached=0;
    }
    if(cached)goto execute_cached;

    for(unsigned n=0;n<8u&&n<budget;n++) {
        uint32_t at=(off+n*8u)&micro_mask;
        const uint8_t *q=micro+at;
        lo[n]=(uint32_t)q[0]|((uint32_t)q[1]<<8)|((uint32_t)q[2]<<16)|((uint32_t)q[3]<<24);
        up[n]=(uint32_t)q[4]|((uint32_t)q[5]<<8)|((uint32_t)q[6]<<16)|((uint32_t)q[7]<<24);
        unsigned op=lo[n]>>25;
        if((up[n]&0x7e000000u)||
           (!(up[n]&0x80000000u)&&(op>=0x20u&&op<=0x2fu || vu_lower_pipeline_boundary(lo[n]))))break;
        count++;
    }
    if(count<2u)return 0;

    int hit=p->valid&&p->micro==micro&&p->pc==off&&p->mask==micro_mask&&p->count<=count;
    for(unsigned n=0;hit&&n<p->count;n++)if(p->upper[n]!=up[n]||p->lower[n]!=lo[n])hit=0;
    if(!hit) {
        pair_fn fn=NULL;unsigned compiled=count;
        for(;compiled>=2u;compiled--) {
            ppc_codegen_ctx_t c;if(ppc_dynarec_init(&c,4u*compiled+2u))return 0;
            int result=ppc_dynarec_translate_vu_block(&c,up,lo,compiled);
            if(result==-2){ppc_dynarec_free(&c);return 0;}
            if(!result){fn=(pair_fn)ppc_dynarec_finalize(&c);if(!fn){ppc_dynarec_free(&c);return 0;}break;}
            ppc_dynarec_free(&c);
        }
        if(p->fn)free((void*)p->fn);
        p->micro=micro;p->pc=off;p->mask=micro_mask;p->count=(uint8_t)(fn?compiled:count);p->valid=1;p->fn=fn;
        memcpy(p->upper,up,p->count*sizeof(uint32_t));memcpy(p->lower,lo,p->count*sizeof(uint32_t));
    }
execute_cached:
    if(!p->fn)return 0;
    p->fn(vf,vi,acc,mem,mask,off,delay,target);
    *pc=(off+p->count*8u)&micro_mask;*retired+=p->count;
    block_count++;pair_count+=p->count;upper_count+=p->count;
    for(unsigned n=0;n<p->count;n++)if(!(p->upper[n]&0x80000000u))lower_count++;
    return p->count;
#else
    (void)vf;(void)vi;(void)acc;(void)mem;(void)mask;(void)micro;(void)micro_mask;
    (void)pc;(void)delay;(void)target;(void)ebit;(void)retired;(void)budget;return 0;
#endif
}
uint64_t vu_jit_get_block_count(void){return block_count;}
uint64_t vu_jit_get_pair_count(void){return pair_count;}
uint64_t vu_jit_get_upper_count(void){return upper_count;}
uint64_t vu_jit_get_lower_count(void){return lower_count;}
uint64_t vu_jit_get_rejected_hit_count(void){return rejected_hits;}
uint32_t vu_jit_get_cache_size(void){return owned_count;}
void vu_jit_reset_for_test(void) {
#if defined(GEKKO) && !defined(PCSX2WII_JIT_DISABLE)
    for(unsigned n=0;n<VU_CODE_SLOTS;n++)if(owned[n].fn)free((void*)owned[n].fn);
    for(unsigned n=0;n<VU_PAIR_SLOTS;n++)if(pairs[n].fn)free((void*)pairs[n].fn);
    for(unsigned n=0;n<VU_BLOCK_SLOTS;n++)if(blocks[n].fn)free((void*)blocks[n].fn);
    memset(blocks,0,sizeof blocks);
    memset(pairs,0,sizeof pairs);
    memset(owned,0,sizeof owned);memset(lookup,0,sizeof lookup);
#endif
    upper_count=lower_count=rejected_hits=pair_count=block_count=0;owned_count=0;
}
