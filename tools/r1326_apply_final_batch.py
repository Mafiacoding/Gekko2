#!/usr/bin/env python3
"""Apply the first R1326 final-dynarec batch with guarded exact anchors.

This script deliberately edits the checked-out source only.  The verification
workflow will run the focused audits and the full regression suite before the
resulting source is committed/pushed.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def replace_once(path, old, new):
    p = ROOT / path
    text = p.read_text()
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected exactly one guarded anchor, found {count}")
    p.write_text(text.replace(old, new, 1))

# -------------------------------------------------------------------------
# Phase 1: VU micro side-effect boundaries + the real VI[R] LFSR family.
# -------------------------------------------------------------------------
replace_once(
    "source/hw/vu.c",
    """                return 0; /* R-group (RNEXT/RGET/RINIT/RXOR - needs a real LFSR, not modeled), XTOP/XITOP (needs real VIF1 TOP register plumbing), and anything else unmatched */\n""",
    """                if (fdslot == VULS_FD_R_GROUP) {\n                    /* Real VU R register lives at VI[20] (REG_R).  PCSX2's\n                     * interpreter keeps it in 1.x IEEE mantissa form: the\n                     * exponent is forced to 0x3f800000 and only 23 mantissa\n                     * bits participate in the LFSR/XOR operations.  All four\n                     * R instructions are architecturally suppressed when Ft\n                     * is VF0, including RINIT/RXOR's REG_R side effect. */\n                    if (rt == 0) return 1;\n                    uint32_t r = vi[20];\n                    if (bc2 == 0) { /* RNEXT */\n                        uint32_t x = (r >> 4) & 1u, y = (r >> 22) & 1u;\n                        r = (r << 1) ^ x ^ y;\n                        r = (r & 0x007fffffu) | 0x3f800000u;\n                        vi[20] = r;\n                        float rr[4] = { vu_f(r), vu_f(r), vu_f(r), vu_f(r) };\n                        vu_write_dest(vf, rt, dest, rr);\n                        return 1;\n                    }\n                    if (bc2 == 1) { /* RGET */\n                        float rr[4] = { vu_f(r), vu_f(r), vu_f(r), vu_f(r) };\n                        vu_write_dest(vf, rt, dest, rr);\n                        return 1;\n                    }\n                    if (bc2 == 2) { /* RINIT */\n                        uint32_t elem = (w >> 21) & 3u;\n                        vi[20] = (vf[rs][elem] & 0x007fffffu) | 0x3f800000u;\n                        return 1;\n                    }\n                    if (bc2 == 3) { /* RXOR */\n                        uint32_t elem = (w >> 21) & 3u;\n                        vi[20] = ((vf[rs][elem] ^ r) & 0x007fffffu) | 0x3f800000u;\n                        return 1;\n                    }\n                }\n                return 0; /* Any genuinely unmatched SPECIAL2 selector stays on the explicit unimplemented path. */\n"""
)

replace_once(
    "source/core/recompiler/vu_jit.c",
    """#include \"core/recompiler/vu_jit.h\"\n#include \"core/recompiler/ppc_dynarec.h\"\n""",
    """#include \"core/recompiler/vu_jit.h\"\n#include \"core/recompiler/ppc_dynarec.h\"\n#include \"core/hw/vu_opcodes.h\"\n"""
)

replace_once(
    "source/core/recompiler/vu_jit.c",
    """static inline vu_fn hot_lookup(uint32_t word,unsigned lower) {\n    lookup_entry *p=&lookup[hash(word,lower,VU_LOOKUP_SLOTS-1u)];\n    if(p->valid && p->word==word && p->lower==lower) {\n        if(p->rejected)rejected_hits++;\n        return p->fn;\n    }\n    return resolve(word,lower);\n}\n\n#endif\n""",
    """static inline vu_fn hot_lookup(uint32_t word,unsigned lower) {\n    lookup_entry *p=&lookup[hash(word,lower,VU_LOOKUP_SLOTS-1u)];\n    if(p->valid && p->word==word && p->lower==lower) {\n        if(p->rejected)rejected_hits++;\n        return p->fn;\n    }\n    return resolve(word,lower);\n}\n\n/* Scheduler-visible lower operations are hard native-block boundaries.\n * Pair/single execution may still JIT ordinary work, but a multi-pair block\n * must return to vu_micro_step_pipeline() before Q/P publication, WAITQ/P,\n * VIF TOP/ITOP reads, PATH1 XGKICK, MFP/P reads or REG_R LFSR mutation.\n * Keeping the lower word as data when the upper I bit is set is intentional. */\nstatic int vu_lower_pipeline_boundary(uint32_t word)\n{\n    if ((word >> 25) != VUL_OP_SPECIAL || (word & 63u) < VULS_FUNCT_MIN)\n        return 0;\n    unsigned fd = (word >> 6) & 31u;\n    return fd == VULS_FD_DIVQ_GROUP || fd == VULS_FD_R_GROUP || fd >= 0x19u;\n}\n\n#endif\n"""
)

replace_once(
    "source/core/recompiler/vu_jit.c",
    """        unsigned op=lo[n]>>25;\n        if((up[n]&0x7e000000u)||(!(up[n]&0x80000000u)&&op>=0x20u&&op<=0x2fu))break;\n        count++;\n""",
    """        unsigned op=lo[n]>>25;\n        if((up[n]&0x7e000000u)||\n           (!(up[n]&0x80000000u)&&(op>=0x20u&&op<=0x2fu || vu_lower_pipeline_boundary(lo[n]))))break;\n        count++;\n"""
)

# -------------------------------------------------------------------------
# Phase 2: extend the existing write-through residency planner to all four
# 32-bit words of selected EE GPRs in MMI-heavy blocks.  The canonical EE
# context is still written through at every store/helper/exit, so this adds
# no new dirty-state contract; it only lets the already-proven rewriter bind
# offsets +0/+4/+8/+12 to r18-r29.
# -------------------------------------------------------------------------
replace_once(
    "source/core/recompiler/ppc_residency_internal.h",
    """    unsigned reads[32]={0},writes[32]={0};\n    memset(a,0,sizeof(*a));memset(a->slot,-1,sizeof a->slot);\n    a->bank_words=ee?128u:32u;\n    a->generation_offset=generation_offset;\n    for(unsigned n=0;n<count;n++) {\n        uint32_t w=words[n];unsigned op=w>>26,rs=(w>>21)&31u,rt=(w>>16)&31u,rd=(w>>11)&31u,fn=w&63u;\n""",
    """    unsigned reads[32]={0},writes[32]={0},has_mmi=0;\n    memset(a,0,sizeof(*a));memset(a->slot,-1,sizeof a->slot);\n    a->bank_words=ee?128u:32u;\n    a->generation_offset=generation_offset;\n    if(ee)for(unsigned n=0;n<count;n++)if((words[n]>>26)==0x1cu){has_mmi=1;break;}\n    for(unsigned n=0;n<count;n++) {\n        uint32_t w=words[n];unsigned op=w>>26,rs=(w>>21)&31u,rt=(w>>16)&31u,rd=(w>>11)&31u,fn=w&63u;\n"""
)

replace_once(
    "source/core/recompiler/ppc_residency_internal.h",
    """        if(!(op==0u||(op>=8u&&op<=14u)||op==25u))continue;\n        if(op==0u&&(fn==8u||fn==9u||fn==10u||fn==11u||fn==0x1au||fn==0x1bu))continue;\n        if(rs)reads[rs]++;\n        if(op==0u) {\n""",
    """        if(!(op==0u||(op>=8u&&op<=14u)||op==25u||(ee&&op==0x1cu)))continue;\n        if(op==0u&&(fn==8u||fn==9u||fn==10u||fn==11u||fn==0x1au||fn==0x1bu))continue;\n        if(rs)reads[rs]++;\n        if(ee&&op==0x1cu) {\n            /* MMI register fields are intentionally over-approximated here.\n             * Allocation choice may load an unused lane, but emitted-code\n             * rewriting is still keyed by the exact architectural offset,\n             * so an over-approximation cannot alter instruction semantics. */\n            if(rt)reads[rt]++;\n            if(rd)writes[rd]++;\n            continue;\n        }\n        if(op==0u) {\n"""
)

replace_once(
    "source/core/recompiler/ppc_residency_internal.h",
    """    for(unsigned k=0;k<(ee?6u:12u);k++) {\n        unsigned best=0;\n        for(unsigned r=1;r<32;r++)\n            if(reads[r]>=2u&&reads[r]>writes[r]&&(!best||reads[r]-writes[r]>reads[best]-writes[best]))best=r;\n        if(!best)break;\n        reads[best]=0;\n        for(unsigned lane=0;lane<(ee?2u:1u);lane++) {\n            unsigned word=ee?best*4u+lane:best;\n            a->slot[word]=(int8_t)a->count;a->offsets[a->count++]=(uint16_t)(word*4u);\n        }\n    }\n""",
    """    unsigned lanes=ee?(has_mmi?4u:2u):1u;\n    unsigned max_guests=12u/lanes;\n    for(unsigned k=0;k<max_guests;k++) {\n        unsigned best=0;\n        for(unsigned r=1;r<32;r++)\n            if(reads[r]>=2u&&reads[r]>writes[r]&&(!best||reads[r]-writes[r]>reads[best]-writes[best]))best=r;\n        if(!best)break;\n        reads[best]=0;\n        for(unsigned lane=0;lane<lanes;lane++) {\n            unsigned word=ee?best*4u+lane:best;\n            a->slot[word]=(int8_t)a->count;a->offsets[a->count++]=(uint16_t)(word*4u);\n        }\n    }\n"""
)

print("R1326 guarded final-batch patch: phase 1+2 applied")
