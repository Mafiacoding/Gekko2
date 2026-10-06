/* Internal write-through residency for audited EE/IOP block callbacks.
 * Context remains canonical at every helper and exit. r18-r29 retain hot
 * guest words; r30 tracks external GPR mutations. Opaque bodies invalidate
 * compile-time bindings rather than assuming an unknown helper contract. */
typedef struct {
    int8_t slot[128];
    uint16_t offsets[12],valid;
    unsigned count,bank_words,generation_offset;
} ppc_residency;
static uint64_t resident_blocks,resident_loads,resident_refresh_edges;
uint64_t ppc_dynarec_get_resident_blocks(void){return resident_blocks;}
uint64_t ppc_dynarec_get_resident_loads(void){return resident_loads;}
uint64_t ppc_dynarec_get_resident_refresh_edges(void){return resident_refresh_edges;}
static void residency_plan(ppc_residency *a,const uint32_t *words,unsigned count,int ee,unsigned generation_offset)
{
    unsigned reads[32]={0},writes[32]={0},has_mmi=0;
    memset(a,0,sizeof(*a));memset(a->slot,-1,sizeof a->slot);
    a->bank_words=ee?128u:32u;
    a->generation_offset=generation_offset;
    if(ee)for(unsigned n=0;n<count;n++)if((words[n]>>26)==0x1cu){has_mmi=1;break;}
    for(unsigned n=0;n<count;n++) {
        uint32_t w=words[n];unsigned op=w>>26,rs=(w>>21)&31u,rt=(w>>16)&31u,rd=(w>>11)&31u,fn=w&63u;
        /* Only repeated scalar integer sources justify a resident pool.
         * Opaque helpers, merge branches and resolved memory proofs do not
         * reuse those operand loads; a memory-only block stays unmodified. */
        if(!(op==0u||(op>=8u&&op<=14u)||op==25u||(ee&&op==0x1cu)))continue;
        if(op==0u&&(fn==8u||fn==9u||fn==10u||fn==11u||fn==0x1au||fn==0x1bu))continue;
        if(rs)reads[rs]++;
        if(ee&&op==0x1cu) {
            /* MMI register fields are intentionally over-approximated here.
             * Allocation choice may load an unused lane, but emitted-code
             * rewriting is still keyed by the exact architectural offset,
             * so an over-approximation cannot alter instruction semantics. */
            if(rt)reads[rt]++;
            if(rd)writes[rd]++;
            continue;
        }
        if(op==0u) {
            if(rt)reads[rt]++;
            if(rd&&fn!=8u&&fn!=0x11u&&fn!=0x13u)writes[rd]++;
        } else if((op>=8u&&op<=15u)||op==25u||(op>=0x20u&&op<=0x27u)||op==0x1eu) {
            if(rt)writes[rt]++;
        } else if(rt)reads[rt]++;
        if(op==3u)writes[31]++;
    }
    /* Admission avoids paying resident stores for a simple read/write
     * accumulator. Cache genuinely reused sources; all words keep their
     * architectural offsets; uncached upper EE words remain canonical. */
    unsigned lanes=ee?(has_mmi?4u:2u):1u;
    unsigned max_guests=12u/lanes;
    for(unsigned k=0;k<max_guests;k++) {
        unsigned best=0;
        for(unsigned r=1;r<32;r++)
            if(reads[r]>=2u&&reads[r]>writes[r]&&(!best||reads[r]-writes[r]>reads[best]-writes[best]))best=r;
        if(!best)break;
        reads[best]=0;
        for(unsigned lane=0;lane<lanes;lane++) {
            unsigned word=ee?best*4u+lane:best;
            a->slot[word]=(int8_t)a->count;a->offsets[a->count++]=(uint16_t)(word*4u);
        }
    }
}
static void residency_save(ppc_codegen_ctx_t *ctx,const ppc_residency *a,int restore)
{
    if(!a->count)return;
    for(unsigned n=0;n<a->count;n++)emit(ctx,restore?enc_lwz(18+(int)n,1,60+(int)n*4):enc_stw(18+(int)n,1,60+(int)n*4));
    emit(ctx,restore?enc_lwz(30,1,108):enc_stw(30,1,108));
}
static void residency_refresh(ppc_codegen_ctx_t *ctx,ppc_residency *a)
{
    if(!a->count)return;
    emit(ctx,enc_lwz(12,3,(int16_t)a->generation_offset));
    if(a->valid) {
        emit(ctx,enc_cmplw(12,30));size_t same=ctx->used_words;emit(ctx,enc_bc(12,2,0));
        for(unsigned n=0;n<a->count;n++)if(a->valid&(1u<<n))emit(ctx,enc_lwz(18+(int)n,3,(int16_t)a->offsets[n]));
        emit(ctx,enc_or(30,12,12));
        ctx->code[same]=enc_bc(12,2,(int32_t)(ctx->used_words-same)*4);resident_refresh_edges++;
    } else emit(ctx,enc_or(30,12,12));
}
static unsigned residency_reg(const int8_t *map,unsigned r){return (unsigned)map[r];}
/* Pure straight-line bodies alone are renamed. Unknown register effects,
 * helper calls, byte-lane writes and local branches are explicit fences. */
static int residency_body(ppc_codegen_ctx_t *ctx,size_t begin,ppc_residency *a)
{
    size_t count=ctx->used_words-begin;if(!a->count||!count)return 0;
    for(size_t n=0;n<count;n++) {
        uint32_t w=ctx->code[begin+n];unsigned op=w>>26;int dest=word_alloc_destination(w);
        if(op==16u||op==18u||op==19u||(dest==-2&&op!=36u)||dest==3||dest>=18) {a->valid=0;return 0;}
    }
    ppc_codegen_ctx_t out;if(ppc_dynarec_init(&out,(count+24u)/128u+2u)){a->valid=0;return 0;}
    int8_t map[32];for(unsigned r=0;r<32;r++)map[r]=(int8_t)r;
    uint16_t valid=a->valid;unsigned removed=0;
    for(size_t n=0;n<count;n++) {
        uint32_t w=ctx->code[begin+n];unsigned op=w>>26,rt=(w>>21)&31u,ra=(w>>16)&31u,rb=(w>>11)&31u,xo=(w>>1)&1023u;
        int off=(int16_t)w;int bank=ra==3u&&off>=0&&(off&3)==0&&(unsigned)off/4u<a->bank_words;
        int slot=bank?a->slot[(unsigned)off/4u]:-1;
        if(op==32u&&slot>=0&&rt>=4u&&rt<=11u) {
            if(valid&(1u<<slot))removed++;
            else {emit(&out,enc_lwz(18+slot,3,(int16_t)off));valid|=(uint16_t)(1u<<slot);}
            map[rt]=(int8_t)(18+slot);continue;
        }
        unsigned mrt=residency_reg(map,rt),mra=residency_reg(map,ra),mrb=residency_reg(map,rb);
        int dest=word_alloc_destination(w);
        if(op==36u) {
            emit(&out,enc_stw((int)mrt,ra?(int)mra:0,(int16_t)off));
            if(slot>=0) {
                unsigned host=18u+(unsigned)slot;
                if(mrt!=host) {
                    /* Other temporaries may still hold the old operand. */
                    for(unsigned t=4;t<=11;t++)if((unsigned)map[t]==host) {emit(&out,enc_or((int)t,(int)host,(int)host));map[t]=(int8_t)t;}
                    emit(&out,enc_or((int)host,(int)mrt,(int)mrt));
                }
                valid|=(uint16_t)(1u<<slot);
            }
            continue;
        }
        if(op==7u||op==8u||op==12u||op==13u||op==14u||op==15u||op==32u||op==34u||op==40u||op==42u) {
            if(ra)w=(w&~0x001f0000u)|(mra<<16);
        } else if(op==10u||op==11u)w=(w&~0x001f0000u)|(mra<<16);
        else if(op==20u||op==21u||op==23u||(op>=24u&&op<=29u)) {
            if(op==20u&&mra!=ra)emit(&out,enc_or((int)ra,(int)mra,(int)mra));
            w=(w&~0x03e00000u)|(mrt<<21);
            if(op==23u)w=(w&~0x0000f800u)|(mrb<<11);
        } else if(op==31u) {
            switch(xo) {
            case 19:case 339:case 512:break;
            case 144:case 467:w=(w&~0x03e00000u)|(mrt<<21);break;
            case 24:case 28:case 60:case 124:case 284:case 316:case 412:case 444:case 536:case 792:
                w=(w&~0x03e0f800u)|(mrt<<21)|(mrb<<11);break;
            case 824:case 922:case 954:w=(w&~0x03e00000u)|(mrt<<21);break;
            default:w=(w&~0x001ff800u)|(mra<<16)|(mrb<<11);break;
            }
        }
        emit(&out,w);if(dest>=0)map[dest]=(int8_t)dest;
    }
    if(begin+out.used_words<=ctx->capacity_words) {
        memcpy(ctx->code+begin,out.code,out.used_words*4u);ctx->used_words=begin+out.used_words;a->valid=valid;resident_loads+=removed;
    } else a->valid=0;
    ppc_dynarec_free(&out);return 1;
}
