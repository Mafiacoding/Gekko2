#include "core/recompiler/cache_profile.h"
#include "core/recompiler/ppc_code_cache.h"
#include "core/recompiler/dynarec_config.h"
/*
 * ee_jit.c - see include/core/recompiler/ee_jit.h for the full design
 * rationale and safety model. Short version: this compiles single
 * pure-ALU MIPS instructions into cached, natively-executing PPC750
 * machine code (via ppc_dynarec.c), keyed by instruction ENCODING
 * (not address, since none of the supported opcodes are memory- or
 * PC-dependent) so the same compiled block is reused for every future
 * occurrence of that exact instruction word anywhere in memory.
 */

#include "core/recompiler/ee_jit.h"
#include "core/recompiler/ppc_dynarec.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h> /* offsetof, for the _Static_assert layout checks below */

/* Round 890 (task #874): compile-time enforcement of the layout
 * contract ppc_dynarec.c's HI_IDX/LO_IDX (see that file) depends on:
 * `gpr[32]` must be the very FIRST field of ee_state_t (so `&st->gpr[0]`
 * is also byte offset 0 of the whole struct - this was already relied
 * on before this round, just never asserted), and `hi`/`lo` must sit
 * immediately after it with no padding, so that ppc_dynarec.c's flat
 * "register 32 = HI, register 33 = LO" addressing lands exactly on the
 * real fields. If ee_core.h's struct layout ever changes, this fires a
 * compile error here instead of silently corrupting HI/LO (or some
 * unrelated field) the next time a MULT/DIV/MFHI/MTHI/MFLO/MTLO
 * instruction gets JIT-compiled. */
_Static_assert(offsetof(ee_state_t,cop0)==556,"R1281 native COP0 prefix offset changed");
_Static_assert(offsetof(ee_state_t, gpr) == 0,
               "ppc_dynarec.c's context pointer is &st->gpr[0] - gpr must be ee_state_t's first field");
_Static_assert(offsetof(ee_state_t, hi) == sizeof(ee_reg128_t) * 32,
               "ppc_dynarec.c's HI_IDX (32) assumes hi sits immediately after gpr[32]");
_Static_assert(offsetof(ee_state_t, lo) == sizeof(ee_reg128_t) * 33,
               "ppc_dynarec.c's LO_IDX (33) assumes lo sits immediately after hi");

/* Round 894 (task #878): J/JAL/JR/JALR are the first opcodes whose
 * generated code touches ee_state_t fields OTHER than the flat
 * gpr[32]+hi+lo register array - they need to read `exc_this_pc` (to
 * compute J/JAL's absolute target at RUNTIME, since this dynarec's
 * cache is keyed by instruction encoding, not address - see
 * ppc_dynarec.c's EXC_THIS_PC_OFFSET comment) and write `next_pc`/
 * `branch_pending` (to hand control-flow back to ee_step() exactly the
 * way its own BRANCH_TO() macro does). This works because CTX_REG
 * (r3) is `&st->gpr[0]`, which - per the _Static_assert above - is
 * also byte offset 0 of the WHOLE ee_state_t struct, so any field is
 * reachable as a plain lwz/stw/stb at its real offset. These three
 * asserts pin those offsets against ee_core.h's actual layout at
 * COMPILE TIME (on whichever target actually builds this file, host or
 * GEKKO, so any host/PPC struct-padding difference would be caught
 * here too) - if ee_core.h ever moves pc/next_pc/branch_pending/
 * exc_this_pc, this fires a compile error instead of J/JAL/JR/JALR
 * silently corrupting control flow the next time one gets JIT-compiled. */
_Static_assert(offsetof(ee_state_t, next_pc) == 548,
               "ppc_dynarec.c's NEXT_PC_OFFSET assumes next_pc sits at this exact byte offset");
_Static_assert(offsetof(ee_state_t, branch_pending) == 684,
               "ppc_dynarec.c's BRANCH_PENDING_OFFSET assumes branch_pending sits at this exact byte offset");
_Static_assert(offsetof(ee_state_t, exc_this_pc) == 1456,
               "ppc_dynarec.c's EXC_THIS_PC_OFFSET assumes exc_this_pc sits at this exact byte offset");
/* Round 896 (task #880): the "likely" branches (BLTZL/BGEZL/BEQL/BNEL/
 * BLEZL/BGTZL) are this dynarec's first opcodes to WRITE ee_state_t.pc
 * directly (every prior branch/jump opcode only ever wrote next_pc/
 * branch_pending) - see ppc_dynarec.c's PC_OFFSET comment for why the
 * not-taken/annulled case needs this. */
_Static_assert(offsetof(ee_state_t, pc) == 544,
               "ppc_dynarec.c's PC_OFFSET assumes pc sits at this exact byte offset");
/* R1249: pointer-dependent offsets describe the 32-bit PPC ABI only.
 * Host interpreter builds use 64-bit pointers and never execute PPC code. */
#ifdef GEKKO
/* R1173: direct generated-PPC KSEG0/KSEG1 RAM load fast path. */
_Static_assert(offsetof(ee_state_t, ram) == 10480, "R1173 RAM_PTR_OFFSET layout mismatch");
_Static_assert(offsetof(ee_state_t, ram_size) == 10484, "R1173 RAM_SIZE_OFFSET layout mismatch");
#endif
_Static_assert(offsetof(ee_state_t, mem_tlb_miss) == 1462, "R1173 MEM_TLB_MISS_OFFSET layout mismatch");

/* Round 902 (task #884): MFC1/CFC1/MTC1/CTC1/MOV.S/ABS.S/NEG.S are this
 * dynarec's first opcodes to touch ee_state_t's COP1 (FPU) fields. */
_Static_assert(offsetof(ee_state_t, fpr) == 1464,
               "ppc_dynarec.c's REG_FPR() assumes fpr[0] sits at this exact byte offset");
_Static_assert(offsetof(ee_state_t, fcr31) == 1592,
               "ppc_dynarec.c's FCR31_OFFSET assumes fcr31 sits at this exact byte offset");
_Static_assert(offsetof(ee_state_t, acc) == 1596,
               "ppc_dynarec.c's ACC_OFFSET assumes acc sits at this exact byte offset");

/* Round 907 (task #892): VADD/VSUB/VMUL (COP2/VU0 macro mode) are this
 * dynarec's first opcodes to touch ee_state_t's VU0 vector-register
 * file (vu0_vf[32][4], distinct from and much larger than the scalar
 * COP1 fpr[32] above). */
_Static_assert(offsetof(ee_state_t, vu0_vf) == 1728,
               "ppc_dynarec.c's VU0_VF_OFF() assumes vu0_vf[0][0] sits at this exact byte offset");
_Static_assert(offsetof(ee_state_t, vu0_mem) == 2240,
               "ppc_dynarec.c's VU0_MEM_OFFSET assumes vu0_mem[0] sits at this exact byte offset");

#define EE_JIT_CACHE_SLOTS 8192u /* power of two - see ee_jit_cache_lookup()/insert() */

typedef struct {
    uint32_t     instr; /* only meaningful when fn != NULL */
    ppc_block_fn fn;    /* NULL = empty slot */
} ee_jit_cache_slot_t;

static ee_jit_cache_slot_t g_cache[EE_JIT_CACHE_SLOTS];
static uint32_t g_cache_count = 0;
static uint64_t g_jit_executed = 0;

/* R1161: tiny direct-mapped L1 in front of the open-addressed cache.
 * R1160 measured ~28.26M cache hits for only ~2K misses, so the hot cost is
 * no longer compilation but doing the full hash/probe lookup on virtually
 * every EE instruction.  This L1 is only a lookup accelerator: it never owns
 * code buffers and a miss falls through to the exact old cache path. */
#define EE_JIT_L1_SLOTS 2048u
typedef struct { uint32_t instr; ppc_block_fn fn; } ee_jit_l1_slot_t;
static ee_jit_l1_slot_t g_l1[EE_JIT_L1_SLOTS];

/* R1168: enlarged hot caches after on-target profiling showed millions of
 * avoidable L0 conflict misses.  Keep exact R1162 semantics/SMC tagging; only
 * capacity changes (L0 1K -> 16K, L1 256 -> 2K).
 *
 * R1162: PC-tagged L0 trace cache.  R1160/R1161 showed >93% of EE
 * instructions already execute through the JIT and that the code cache is
 * overwhelmingly hot.  The remaining front-end work on every instruction
 * was still: decode the opcode enough for ee_jit_opcode_supported(), hash
 * the 32-bit instruction word, then probe the instruction-keyed L1.
 *
 * A real multi-instruction block JIT cannot simply skip ee_step()'s
 * per-instruction epilogue: this tree has prior regressions proving timer,
 * VBLANK, RPC and interrupt cadence matters at each genuine EE boundary.
 * This L0 is the safe first half of block formation: key by execution PC and
 * cache the already-resolved native function, while STILL executing exactly
 * one EE instruction and the exact old epilogue per call.  `instr` is part
 * of the tag and is re-fetched by ee_step() before this lookup, so SMC/overlay
 * replacement is detected immediately and falls back to the normal resolver. */
#define EE_JIT_PC_L0_SLOTS 16384u
typedef struct { uint32_t pc, instr; ppc_block_fn fn; uint8_t rejected; } ee_jit_pc_l0_slot_t;
static ee_jit_pc_l0_slot_t g_pc_l0[EE_JIT_PC_L0_SLOTS];
static uint64_t g_pc_l0_hits = 0, g_pc_l0_misses = 0;
static uint64_t g_pc_l0_rejected_hits = 0, g_compile_attempts = 0;
/* Deterministically unsupported encodings recur at many different PCs. */
typedef struct { uint32_t instr; uint8_t valid; } ee_rejected_slot;
static ee_rejected_slot g_ee_rejected[64];

static inline uint32_t ee_jit_pc_l0_index(uint32_t pc)
{
    return ((pc >> 2) ^ (pc >> 12)) & (EE_JIT_PC_L0_SLOTS - 1u);
}

static inline uint32_t ee_jit_l1_index(uint32_t instr)
{
    return ((instr >> 2) ^ (instr >> 11) ^ instr) & (EE_JIT_L1_SLOTS - 1u);
}

/* These three helpers are only reachable from the GEKKO branch of
 * ee_jit_try_execute_one() below (see that function's host-safety-gate
 * comment) - wrapped in #ifdef GEKKO here too so host-native builds
 * (which never call them) don't warn about unused static functions. */
#ifdef GEKKO

/* Whether `instr` is one of the MIPS opcodes ppc_dynarec_translate_one()
 * currently supports: ADDIU (op 0x09); SLTI/SLTIU (op 0x0A/0x0B); LUI
 * (op 0x0F, Round 887b); SPECIAL (op 0x00) ADDU/SUBU/AND/OR/XOR/NOR/
 * SLT/SLTU (funct 0x21/0x23-0x27/0x2A/0x2B); SLL/SRL/SRA/SLLV/SRLV/SRAV
 * (funct 0x00/0x02-0x04/0x06/0x07, Round 888); MOVZ/MOVN (funct
 * 0x0A/0x0B, Round 889); MFHI/MTHI/MFLO/MTLO (funct 0x10-0x13) and
 * MULT/MULTU/DIV/DIVU (funct 0x18/0x19/0x1A/0x1B), added in Round 890
 * now that HI/LO have somewhere real to live (see HI_IDX/LO_IDX in
 * ppc_dynarec.c); LW/SW (op 0x23/0x2B), new in Round 891 - this
 * dynarec's first opcodes that call a real C function
 * (ee_mem_read32/ee_mem_write32) rather than just moving bits between
 * the context array and PPC registers (see ppc_dynarec.c's
 * ADDR_EE_MEM_READ32/WRITE32 comment for the call-emission mechanism);
 * LB/LBU/LH/LHU/LWU/SB/SH (op 0x20/0x24/0x21/0x25/0x27/0x28/0x29),
 * new in Round 892 - the same call-emission mechanism extended to the
 * rest of the base-ISA byte/halfword/unsigned-word loads and stores;
 * LD/SD (op 0x37/0x3F), new in Round 893 - the same mechanism one
 * more time, now handling a genuine 64-bit callee value via a PowerPC
 * EABI register PAIR (r3:r4 for LD's return, r5:r6 for SD's argument)
 * instead of a single 32-bit register (see ppc_dynarec.c's
 * ADDR_EE_MEM_READ64/WRITE64 comment) - this completes the full
 * base-ISA integer load/store family; and J/JAL (op 0x02/0x03) and
 * JR/JALR (SPECIAL funct 0x08/0x09), new in Round 894 - the full set
 * of unconditional control-transfer opcodes, this dynarec's first
 * opcodes that touch ee_state_t fields other than the gpr/hi/lo
 * register array (exc_this_pc/next_pc/branch_pending - see
 * ppc_dynarec.c's EXC_THIS_PC_OFFSET/NEXT_PC_OFFSET/
 * BRANCH_PENDING_OFFSET comment for why that's safe under this
 * dynarec's instruction-encoding-keyed cache). BEQ/BNE/BLEZ/BGTZ (op
 * 0x04-0x07), new in Round 895 - four of the six non-REGIMM conditional
 * branches, done WITHOUT any real PPC branch instruction (an all-0s/
 * all-1s "taken" mask blended into next_pc/branch_pending, the same
 * technique MOVZ/MOVN already used for conditional register writes -
 * see ppc_dynarec.c's emit_branch_blend() comment). BLTZ/BGEZ (REGIMM,
 * op 0x01, gated on rt==0x00/0x01 - REGIMM's rt field selects which of
 * several unrelated sub-opcodes this really is, so unlike every opcode
 * above it can't be pre-filtered on op alone) and the "likely" variants
 * of every conditional branch (BLTZL/BGEZL via REGIMM rt==0x02/0x03,
 * BEQL/BNEL op 0x14/0x15, BLEZL/BGTZL op 0x16/0x17), new in Round 896 -
 * these reuse emit_branch_blend_likely(), a 22-instruction sibling of
 * emit_branch_blend() that additionally blends ee_state_t.pc itself on
 * the not-taken path, matching the real R5900's delay-slot-annulment
 * semantics for "likely" branches (see ppc_dynarec.c's PC_OFFSET and
 * emit_branch_blend_likely() comments). ADDI (op 0x08, joins ADDIU
 * unchanged - no overflow trap, matching ee_core.c's own documented
 * simplification) and ANDI/ORI/XORI (op 0x0C-0x0E, zero-extended-
 * immediate 64-bit bitwise ops), new in Round 897 - the remaining ALU
 * immediates, closing out the base-ISA immediate-arithmetic/logical
 * family (SLTI/SLTIU/LUI were already covered; DADDI/DADDIU's 64-bit
 * family is scoped for a future round alongside DADD/DSUB/DSLL/DSRL/
 * DSRA). DADD/DADDU/DSUB/DSUBU (funct 0x2C-0x2F) and DSLL/DSRL/DSRA
 * (funct 0x38/0x3A/0x3B), new in Round 898 - the first genuinely
 * 64-bit-native register-register opcodes this dynarec compiles (no
 * 32-bit-compute-then-sign-extend shortcut available, unlike ADDU/SUBU/
 * SLL/SRL/SRA above): synthesized from 32-bit PPC750 primitives via the
 * standard multi-word add-with-carry/subtract-with-borrow idiom (new
 * addc/adde encoders, paired with the existing subfc/subfe from Round
 * 886) for DADD/DSUB, and a "shift each half, OR in the bits that
 * crossed the hi/lo boundary from the other half" idiom (reusing Round
 * 888's existing slw/srw/sraw encoders, no new ones needed) for DSLL/
 * DSRL/DSRA - see ppc_dynarec.c's own comments on both dispatch blocks
 * for the full derivation. DSLL32/DSRL32/DSRA32 (the sa+32 range) are
 * NOT included this round, left for a future increment.
 *
 * LWL/LWR/SWL/SWR (op 0x22/0x26/0x2A/0x2E) and LQ/SQ (op 0x1E/0x1F),
 * new in Round 900 - the EE-specific unaligned-word and 128-bit load/
 * store family. LWL/LWR/SWL/SWR reproduce ee_core.c's per-shift lookup
 * tables as runtime register arithmetic instead of literal tables (see
 * ppc_dynarec.c's own dispatch-block comment for the exact formulas);
 * SWL/SWR are this dynarec's first opcodes that call a real C function
 * TWICE in one block (read-merge-write); LQ/SQ are its first that call
 * ee_mem_read64/write64 twice in one block, to cover the EE's real
 * 128-bit register width (a new REG_HI1/REG_LO1 addressing pair reaches
 * the upper 64 bits, `ud1`, that every earlier opcode left completely
 * alone).
 *
 * COP1/FPU data-movement and bit-level opcodes (op 0x11, `rs` selecting
 * MFC1/CFC1/MTC1/CTC1, or `rs`==0x10/COP1.S with `funct` selecting ABS.S/
 * MOV.S/NEG.S), new in Round 902 - deliberately scoped to exclude any
 * opcode needing real floating-point arithmetic (ADD.S/SUB.S/MUL.S/
 * DIV.S/SQRT.S/etc, plus CVT.W.S/CVT.S.W and the BC1 branch family),
 * which need genuine PPC750 FPU instructions and PCSX2's own overflow/
 * underflow clamping ported faithfully - saved for a later round in this
 * arc. Every opcode here operates on FPR/GPR/FCR31 raw 32-bit bit
 * patterns with plain integer loads/stores/logical ops, matching
 * ee_core.c's own case bodies exactly - no float hardware touched at
 * all. New REG_FPR()/FCR31_OFFSET/ACC_OFFSET addressing constants reach
 * ee_state_t's COP1 fields the same "any field is a plain lwz/stw at its
 * real offset from &gpr[0]" way as every other *_OFFSET constant.
 *
 * Kept in sync by hand with translate_one()'s own dispatch - see
 * that function's own comments for the authoritative list. This is a
 * cheap pre-filter so the (much more expensive) cache lookup/compile
 * path is never attempted for the vast majority of real instructions
 * ppc_dynarec.c can't handle yet (branches, MMI, COP0/2, FPU arithmetic,
 * ...). */
_Static_assert(offsetof(ee_state_t,sa_reg)==552,"EE SA prefix layout");
static int ee_jit_opcode_supported(uint32_t instr)
{
    uint32_t op = (instr >> 26) & 0x3Fu;
    if(op==0x10u){
        unsigned rs=(instr>>21)&31u,rd=(instr>>11)&31u;
        /* R1316: MTC0 EntryHi changes the active ASID/mapping. Keep this
         * one form scalar so the authoritative COP0 case bumps the epoch. */
        if(rs==4u&&rd==10u)return 0;
        return rs==0u||rs==4u;
    }
    if (op == 0x18u || op == 0x19u) return 1; /* R1268 DADDI/DADDIU */
    if (op == 0x08u || op == 0x09u) return 1; /* ADDI (Round 897) / ADDIU */
    if (op == 0x0Au || op == 0x0Bu) return 1; /* SLTI / SLTIU */
    if (op == 0x0Cu || op == 0x0Du || op == 0x0Eu) return 1; /* ANDI / ORI / XORI (Round 897) */
    if (op == 0x0Fu) return 1; /* LUI */
    if (op == 0x1Eu || op == 0x1Fu) return 1; /* LQ / SQ (Round 900) */
    if (op == 0x22u || op == 0x26u) return 1; /* LWL / LWR (Round 900) */
    if (op == 0x2Au || op == 0x2Eu) return 1; /* SWL / SWR (Round 900) */
    if (op == 0x20u || op == 0x24u) return 1; /* LB / LBU (Round 892) */
    if (op == 0x21u || op == 0x25u) return 1; /* LH / LHU (Round 892) */
    if (op == 0x23u) return 1; /* LW (Round 891) */
    if (op == 0x27u) return 1; /* LWU (Round 892) */
    if (op == 0x28u || op == 0x29u) return 1; /* SB / SH (Round 892) */
    if (op == 0x2Bu) return 1; /* SW (Round 891) */
    if (op == 0x37u) return 1; /* LD (Round 893) */
    if (op == 0x3Fu) return 1; /* SD (Round 893) */
    if (op == 0x02u) return 1; /* J (Round 894) */
    if (op == 0x03u) return 1; /* JAL (Round 894) */
    if (op == 0x04u || op == 0x05u) return 1; /* BEQ / BNE (Round 895) */
    if (op == 0x06u || op == 0x07u) return 1; /* BLEZ / BGTZ (Round 895) */
    if (op == 0x14u || op == 0x15u) return 1; /* BEQL / BNEL (Round 896) */
    if (op == 0x16u || op == 0x17u) return 1; /* BLEZL / BGTZL (Round 896) */
    if (op == 0x01u) {
        unsigned trap=(instr>>16)&31u;
        if(trap==8u||trap==9u||trap==10u||trap==11u||trap==12u||trap==14u)return 1;
        /* REGIMM: rt selects the real sub-opcode. R1313 admits the
         * four link variants already implemented by ppc_dynarec.c; keeping
         * them blocked here silently forced scalar fallback on Wii. */
        uint32_t rt = (instr >> 16) & 0x1Fu;
        switch (rt) {
        case 0x18: case 0x19: /* R1270 MTSAB/MTSAH */
        case 0x00: case 0x01: /* BLTZ / BGEZ */
        case 0x02: case 0x03: /* BLTZL / BGEZL */
        case 0x10: case 0x11: /* BLTZAL / BGEZAL */
        case 0x12: case 0x13: /* BLTZALL / BGEZALL */
            return 1;
        default:
            return 0;
        }
    }
    if (op == 0x11u) {
        /* COP1 (FPU), new in Round 902, extended Round 903/904/905/906/
         * 906b: like REGIMM above, `rs` selects the real sub-opcode, not
         * a flat op-only dispatch - only the subset ppc_dynarec.c's
         * op==0x11 block actually implements returns 1 here, matching
         * translate_one()'s own `return -1` paths inside this same
         * op==0x11 block exactly - kept in sync by hand, same
         * discipline as every entry above. As of Round 906b every COP1
         * opcode this project's ee_core.c implements at all is
         * JIT-covered - task #884 is closed. */
        uint32_t rs = (instr >> 21) & 0x1Fu;
        if (rs == 0x00u || rs == 0x02u || rs == 0x04u || rs == 0x06u)
            return 1; /* MFC1 / CFC1 / MTC1 / CTC1 */
        if (rs == 0x10u) {
            uint32_t funct = instr & 0x3Fu;
            if (funct == 0x05u || funct == 0x06u || funct == 0x07u)
                return 1; /* ABS.S / MOV.S / NEG.S */
            if (funct == 0x00u || funct == 0x01u || funct == 0x02u)
                return 1; /* ADD.S / SUB.S / MUL.S (Round 903) */
            if (funct == 0x03u)
                return 1; /* DIV.S (Round 904) */
            if (funct == 0x04u || funct == 0x16u)
                return 1; /* SQRT.S / RSQRT.S (Round 905) */
            if (funct == 0x28u || funct == 0x29u)
                return 1; /* MAX.S / MIN.S (Round 905) */
            if (funct == 0x18u || funct == 0x19u || funct == 0x1Au)
                return 1; /* ADDA.S / SUBA.S / MULA.S (Round 906) */
            if (funct == 0x1Cu || funct == 0x1Du)
                return 1; /* MADD.S / MSUB.S (Round 906) */
            if (funct == 0x1Eu || funct == 0x1Fu)
                return 1; /* MADDA.S / MSUBA.S (Round 906) */
            if (funct == 0x32u || funct == 0x34u || funct == 0x36u)
                return 1; /* C.EQ.S / C.LT.S / C.LE.S (Round 906) */
            if (funct == 0x24u)
                return 1; /* CVT.W.S (Round 906b) */
        }
        if (rs == 0x14u) {
            uint32_t funct = instr & 0x3Fu;
            if (funct == 0x20u)
                return 1; /* CVT.S.W (Round 906b) */
        }
        if (rs == 0x08u) {
            uint32_t rt = (instr >> 16) & 0x1Fu;
            if (rt == 0x00u || rt == 0x01u || rt == 0x02u || rt == 0x03u)
                return 1; /* BC1F / BC1T / BC1FL / BC1TL (Round 906b) */
        }
        return 0;
    }
    if (op == 0x12u) {
        /* Round 922: broadened from a hand-enumerated funct list (which
         * only ever named VADD/VMUL/VSUB, Round 907) to a blanket
         * "op==0x12 is supported" match. This pre-filter had silently
         * fallen out of sync with translate_one()'s own op==0x12
         * dispatch, which by Round 913 covers nearly all of task #885's
         * COP2/VU0 macro-mode scope: the VADD/VSUB/VMUL family (907),
         * VMAX/VMINI/VOPMSUB/VABS/VCLIP (908), VDIV/VSQRT/VRSQRT (909),
         * VIADD/VISUB/VIAND/VIOR/VMOVE/VMR32 (910), VFTOI/VITOF/
         * VCALLMS/VCALLMSR (911), and the scalar transfer family
         * MFC2/QMFC2/CFC2/MTC2/QMTC2/CTC2 (912) - none of which this
         * pre-filter ever let through, so on real GEKKO hardware every
         * one of those Round-908-through-913 instructions was silently
         * falling back to the interpreter despite translate_one()
         * successfully compiling it. Widening this to a blanket op
         * match (rather than re-enumerating every rs/funct combination
         * here too, which is exactly the hand-sync burden that caused
         * this staleness) is safe: ee_jit_try_execute_one() below
         * already treats a nonzero ppc_dynarec_translate_one() return
         * as "not actually supported, fall back to the interpreter" for
         * every opcode, not just this one - so any op==0x12 sub-opcode
         * translate_one() doesn't yet implement costs one wasted (cheap)
         * ppc_dynarec_init()/translate_one() attempt, not a correctness
         * risk. */
        return 1;
    }
    if (op == 0x1Cu) {
        /* Round 922: same blanket-match rationale as op==0x12 just
         * above, for the MMI (EE multimedia/SIMD) family this pre-filter
         * had ZERO entries for at all - meaning every MMI opcode shipped
         * across the entire Round 914-921 arc (task #886, now closed:
         * PADDx/PSUBx, PMULTx/PDIVx, PAND/POR/PXOR/PNOR, the shift
         * family, the pack/unpack family, the merge family, PMFHI/PMFLO/
         * PMTHI/PMTLO/PMFHL/PMTHL, MFHI1/MTHI1/MFLO1/MTLO1, and PROT3W)
         * was being compiled correctly by translate_one() but was NEVER
         * actually reached on real GEKKO hardware, unconditionally
         * falling back to the interpreter for every single MMI
         * instruction a real game/BIOS executes. A handful of rare MMI
         * opcodes remain genuinely unimplemented by design (MADD/MADDU/
         * MADD1/MADDU1, MULT1/MULTU1/DIV1/DIVU1, QFSRV) - those
         * safely fall through translate_one()'s own `return -1` and the
         * same fallback path as every other not-yet-supported opcode
         * project-wide, same safety argument as op==0x12 above. */
        return 1;
    }
    if (op == 0x00u) {
        uint32_t funct = instr & 0x3Fu;
        switch (funct) {
        case 0x30:case 0x31:case 0x32:case 0x33:case 0x34:case 0x36: /* traps */
        case 0x00: /* SLL (and the all-zero-word NOP encoding, harmlessly - see translate_one's rd==0 guard) */
        case 0x02: case 0x03: /* SRL / SRA */
        case 0x04: case 0x06: case 0x07: /* SLLV / SRLV / SRAV */
        case 0x08: case 0x09: /* JR / JALR (Round 894) */
        case 0x0A: case 0x0B: /* MOVZ / MOVN */
        case 0x10: case 0x11: case 0x12: case 0x13: /* MFHI / MTHI / MFLO / MTLO */
        case 0x18: case 0x19: /* MULT / MULTU */
        case 0x1A: case 0x1B: /* DIV / DIVU */
        case 0x20: case 0x21: case 0x22: case 0x23: /* ADD/ADDU/SUB/SUBU */
        case 0x24: case 0x25: case 0x26: case 0x27: /* AND / OR / XOR / NOR */
        case 0x28: case 0x29: /* R1270 MFSA/MTSA */
        case 0x2A: case 0x2B: /* SLT / SLTU */
        case 0x2C: case 0x2D: case 0x2E: case 0x2F: /* DADD / DADDU / DSUB / DSUBU (Round 898) */
        case 0x14: case 0x16: case 0x17: /* R1268 DSLLV/DSRLV/DSRAV */
        case 0x3C: case 0x3E: case 0x3F: /* R1268 DSLL32/DSRL32/DSRA32 */
        case 0x38: case 0x3A: case 0x3B: /* DSLL / DSRL / DSRA (Round 898) */
            return 1;
        default:
            return 0;
        }
    }
    return 0;
}

/* Instruction-keyed owning cache. Once full, existing entries remain usable;
 * uncached encodings fall back to the interpreter without allocating code.
 * L0 remembers deterministic rejection with both PC and instruction tags.
 * Allocation/finalization failures are transient and are never remembered. */
static ppc_block_fn ee_jit_cache_lookup(uint32_t instr)
{
    uint32_t h = (instr * 2654435761u) & (EE_JIT_CACHE_SLOTS - 1u);
    for (uint32_t probe = 0; probe < EE_JIT_CACHE_SLOTS; probe++) {
        uint32_t slot = (h + probe) & (EE_JIT_CACHE_SLOTS - 1u);
        if (g_cache[slot].fn == NULL)
            return NULL; /* empty slot reached along the probe chain: definitely not cached */
        if (g_cache[slot].instr == instr)
            return g_cache[slot].fn;
    }
    return NULL; /* cache full and not found */
}

static int ee_jit_cache_insert(uint32_t instr, ppc_block_fn fn)
{
    if (g_cache_count >= EE_JIT_CACHE_SLOTS)
        return 0; /* No ownership transfer when full. */
    uint32_t h = (instr * 2654435761u) & (EE_JIT_CACHE_SLOTS - 1u);
    for (uint32_t probe = 0; probe < EE_JIT_CACHE_SLOTS; probe++) {
        uint32_t slot = (h + probe) & (EE_JIT_CACHE_SLOTS - 1u);
        if (g_cache[slot].fn == NULL) {
            g_cache[slot].instr = instr;
            g_cache[slot].fn = fn;
            g_cache_count++;
            return 1;
        }
    }
    return 0;
}

#endif /* GEKKO */

static int ee_jit_resolve_and_execute(ee_state_t *st, uint32_t instr, ppc_block_fn *out_fn, int *out_rejected)
{
    if (out_rejected) *out_rejected = 0;
    if(!gekko2_opt_enabled(GEKKO2_OPT_EE_JIT))return 0;
#if !defined(GEKKO) || defined(PCSX2WII_JIT_DISABLE)
    /* Round 887 host-safety gate: ppc_dynarec.c generates raw PPC750
     * machine code, and the block below CALLS it as a function
     * pointer. This project's regression/test suite builds and runs
     * on an x86_64 host - invoking a buffer of PPC opcode bytes there
     * would execute garbage as x86_64 instructions (undefined
     * behavior / near-certain crash or memory corruption), not a
     * no-op. GEKKO is devkitPPC's own auto-defined macro (also passed
     * explicitly via -DGEKKO in this project's Wii Makefile), so it's
     * true exactly when compiling for the real PPC750/Broadway target
     * this generated code can actually run on. On every other build
     * (host-native tests, host tools) this function must decline
     * immediately so ee_step() falls back to the interpreter - exactly
     * the same behavior as every round before this one. Round 886's
     * r880_ppc_verify.c is the correct way to verify the generated PPC
     * ENCODINGS on host: it interprets the raw bytes in a synthetic
     * PPC model rather than executing them natively. See
     * include/core/recompiler/ee_jit.h's header comment.
     *
     * Round 924 (task #913): PCSX2WII_JIT_DISABLE is a new, additive,
     * off-by-default compile-time toggle (not passed by the normal
     * Makefile - only by a separate one-off `make` invocation with
     * `CFLAGS += -DPCSX2WII_JIT_DISABLE`) that forces this same no-op
     * path even on a real GEKKO build. It exists purely to produce a
     * "JIT off" .dol built from the exact same commit/tree as the
     * normal "JIT on" .dol, for a real, on-target Dolphin/Wii A/B
     * comparison - see docs/STATUS.md's Round 924 entry for how to
     * build and use it. Does not change default behavior at all: a
     * plain `make` is byte-for-byte the same JIT-on build as before
     * this round. */
    (void)st;
    (void)instr;
    return 0;
#else
    if (!ee_jit_opcode_supported(instr)) {
        if (out_rejected) *out_rejected = 1;
        return 0;
    }

    ee_rejected_slot *negative=&g_ee_rejected[(instr^(instr>>16))&63u];
    if(negative->valid && negative->instr==instr) {
        if(out_rejected)*out_rejected=1;
        return 0;
    }
    uint32_t l1i = ee_jit_l1_index(instr);
    ppc_block_fn fn = (g_l1[l1i].fn && g_l1[l1i].instr == instr)
                    ? g_l1[l1i].fn : NULL;
    if (!fn) {
        fn = ee_jit_cache_lookup(instr);
        if (fn) {
            g_l1[l1i].instr = instr;
            g_l1[l1i].fn = fn;
        }
    }
    if (!fn) {
        /* R1267: cached functions keep running when full; new encodings
         * interpret instead of allocating unowned executable buffers. */
        if (g_cache_count >= EE_JIT_CACHE_SLOTS) {
            if (out_rejected) *out_rejected = 1;
            return 0;
        }
        g_compile_attempts++;
        ppc_codegen_ctx_t ctx;
        if (ppc_dynarec_init(&ctx, 1) != 0)
            return 0; /* out of memory - fall back to the interpreter, not fatal */
        int translation=ppc_dynarec_translate_one(&ctx, instr);
        if (translation != 0) {
            /* Blanket COP2/MMI gates intentionally include unimplemented
             * encodings. Remember deterministic rejection at L0; never
             * execute a partially generated block. */
            ppc_dynarec_free(&ctx);
            if(translation==-2)return 0; /* Transient allocation failure retries. */
            negative->instr=instr;negative->valid=1;
            if (out_rejected) *out_rejected = 1;
            return 0;
        }
        fn = ppc_dynarec_finalize(&ctx);
        if (!fn) {
            ppc_dynarec_free(&ctx);
            return 0;
        }
        /* Deliberately NOT ppc_dynarec_free(&ctx) here: that would
         * free the very code buffer `fn` now points into (finalize()
         * returns ctx.code itself, memalign'd, cast to a function
         * pointer). `ctx` is a local struct of plain value/pointer
         * fields, so letting it go out of scope here is harmless -
         * ownership of the code buffer has effectively transferred to
         * the cache (ee_jit_reset_stats_for_test() is the only thing
         * that ever frees these, for host-native test hygiene). */
        if (!ee_jit_cache_insert(instr, fn)) {
            ppc_dynarec_free(&ctx);
            return 0;
        }
        g_l1[l1i].instr = instr;
        g_l1[l1i].fn = fn;
    }

    /* ppc_dynarec_gpr128_t and ee_reg128_t are separately-declared but
     * byte-layout-identical structs ({uint64_t ud0, ud1;} in both) -
     * see ppc_dynarec.h's own header comment, which explicitly names
     * this exact cast as the intended zero-copy integration path. */
    if (out_fn) *out_fn = fn;
    fn((ppc_dynarec_gpr128_t *)&st->gpr[0]);
    g_jit_executed++;
    return 1;
#endif /* GEKKO */
}

int ee_jit_try_execute_one(ee_state_t *st, uint32_t instr)
{
    return ee_jit_resolve_and_execute(st, instr, NULL, NULL);
}

int ee_jit_try_execute_one_at(ee_state_t *st, uint32_t pc, uint32_t instr)
{
#if !defined(GEKKO) || defined(PCSX2WII_JIT_DISABLE)
    (void)pc;
    return ee_jit_resolve_and_execute(st, instr, NULL, NULL);
#else
    if(!gekko2_opt_enabled(GEKKO2_OPT_EE_JIT))return 0;
    uint32_t i = ee_jit_pc_l0_index(pc);
    ee_jit_pc_l0_slot_t *e = &g_pc_l0[i];
    /* R1280: compare PC+encoding once for both positive and negative
     * cache entries. The warm positive path previously repeated both
     * comparisons after checking rejection. Preserve every counter and
     * exact-word validation, including self-modifying guest code. */
    if (e->pc == pc && e->instr == instr) {
        if (e->fn) {
            e->fn((ppc_dynarec_gpr128_t *)&st->gpr[0]);
            g_pc_l0_hits++;
            g_jit_executed++;
            return 1;
        }
        if (e->rejected) {
            g_pc_l0_rejected_hits++;
            return 0;
        }
    }

    g_pc_l0_misses++;
    ppc_block_fn fn = NULL;
    int rejected = 0;
    int ok = ee_jit_resolve_and_execute(st, instr, &fn, &rejected);
    if (ok && fn) {
        e->pc = pc;
        e->instr = instr;
        e->fn = fn;
        e->rejected = 0;
    } else if (rejected) {
        e->pc = pc; e->instr = instr; e->fn = NULL; e->rejected = 1;
    }
    return ok;
#endif
}

uint64_t ee_jit_get_rejected_hit_count(void) { return g_pc_l0_rejected_hits; }
uint64_t ee_jit_get_compile_attempt_count(void) { return g_compile_attempts; }

uint64_t ee_jit_get_executed_count(void) { return g_jit_executed; }
uint32_t ee_jit_get_cache_size(void) { return g_cache_count; }
uint64_t ee_jit_get_pc_l0_hit_count(void) { return g_pc_l0_hits; }
uint64_t ee_jit_get_pc_l0_miss_count(void) { return g_pc_l0_misses; }

static void ee_precise_reset_cache(void);
void ee_jit_reset_stats_for_test(void)
{
    ee_precise_reset_cache();
    for (uint32_t i = 0; i < EE_JIT_CACHE_SLOTS; i++) {
        if (g_cache[i].fn != NULL)
            ppc_code_cache_release((void*)g_cache[i].fn);
    }
    memset(g_cache, 0, sizeof(g_cache));
    memset(g_l1, 0, sizeof(g_l1));
    memset(g_pc_l0, 0, sizeof(g_pc_l0));
    memset(g_ee_rejected, 0, sizeof(g_ee_rejected));
    g_pc_l0_hits = g_pc_l0_misses = g_pc_l0_rejected_hits = g_compile_attempts = 0;
    g_cache_count = 0;
    g_jit_executed = 0;
}

/* Bounded PC/tag cache; first conservative precise EE block integration. */
typedef unsigned (*ee_precise_fn)(ee_state_t *,unsigned,uint32_t);
#define EE_PRECISE_RAM_PAGES (32u*1024u*1024u/4096u)
#define EE_PRECISE_NON_RAM_PAGE UINT32_MAX
typedef struct {
 uint32_t pc,words[8],count;
 uint32_t source_page,source_generation,mapping_generation;
 uint32_t serial; /* R1318: non-zero identity of this installed allocation. */
 /* R1326: lazily learned successor identity.  The serial and live target
  * generation are revalidated before every use, so collision eviction,
  * source writes, TLB/ASID changes and checkpoint/reset cannot jump into a
  * stale executable buffer.  This is a native function link, not a patched
  * code-buffer branch, keeping executable ownership simple and reversible. */
 uint32_t link_pc,link_serial;
 uint16_t link_index;
 ee_precise_fn link_fn;
 ee_precise_fn fn;
} ee_precise_slot;
/* Metadata capacity is independent of the 6 MiB executable-code arena.
 * Reuse keeps 4096 owners; Control retains 256 direct-mapped entries. */
#define EE_PRECISE_REUSE_SETS 1024u
#define EE_PRECISE_CACHE_SLOTS (4u*EE_PRECISE_REUSE_SETS)
static ee_precise_slot precise_cache[EE_PRECISE_CACHE_SLOTS];
static uint8_t precise_victim[EE_PRECISE_REUSE_SETS];
static jit_cache_profile precise_profile;
static uint64_t precise_budget_stats[3];
uint64_t ee_jit_get_budget_cache_stat(unsigned n){return n<3u?precise_budget_stats[n]:0;}
void ee_jit_get_cache_profile(jit_cache_profile *out){if(out)*out=precise_profile;}
/* One Broadway cache line per dispatch tag. Keep instruction arrays and
 * learned-edge bookkeeping off the common successor lookup. */
typedef struct {
 uint32_t pc,count,page,generation,mapping,serial,word;
 ee_precise_fn fn;
} ee_precise_dispatch;
#ifdef GEKKO
_Static_assert(sizeof(ee_precise_dispatch)==32u,"Broadway dispatch tag must fit one cache line");
#endif
static ee_precise_dispatch precise_dispatch[EE_PRECISE_CACHE_SLOTS] __attribute__((aligned(32)));
static uint64_t precise_dispatch_hits;
uint64_t ee_jit_get_dispatch_hits(void){return precise_dispatch_hits;}
static uint32_t precise_page_generation[EE_PRECISE_RAM_PAGES];
static uint32_t precise_mapping_generation;
static uint32_t precise_serial_source;
static uint64_t precise_runs,precise_retired,precise_evictions,precise_direct_link_hits;
static ee_precise_slot *precise_chain_source;
static uint32_t precise_chain_source_serial;
static int precise_active;
static unsigned precise_accounted_remaining;

static inline unsigned ee_precise_cache_stride(void)
{return gekko2_opt_enabled(GEKKO2_OPT_CACHE_REUSE)&&!gekko2_opt_enabled(GEKKO2_OPT_COMPACT_CACHE)?EE_PRECISE_REUSE_SETS:256u;}

static inline unsigned ee_precise_cache_index(uint32_t pc)
{
 unsigned mask=ee_precise_cache_stride()-1u;
 return ((pc>>2)^(pc>>12))&mask;
}

unsigned ee_jit_get_cache_entries(void)
{return gekko2_opt_enabled(GEKKO2_OPT_CACHE_REUSE)?4u*ee_precise_cache_stride():256u;}

/* Lookup never allocates or evicts; native chains pin all cache owners. */
static ee_precise_slot *ee_precise_find(uint32_t pc,int install,unsigned budget)
{
 unsigned set=ee_precise_cache_index(pc),stride=ee_precise_cache_stride(),ways=gekko2_opt_enabled(GEKKO2_OPT_CACHE_REUSE)?4u:1u;
 ee_precise_slot *best=NULL;unsigned largest=0;
 for(unsigned w=0;w<ways;w++) {
  unsigned index=set+w*stride;
  const ee_precise_dispatch *tag=&precise_dispatch[index];
  if(!tag->fn||tag->pc!=pc)continue;
  if(ways==1u)return &precise_cache[index];
  if(tag->count>largest)largest=tag->count;
  if(tag->count<=budget&&(!best||tag->count>precise_dispatch[best-precise_cache].count))
   best=&precise_cache[index];
 }
 if(best){if(install&&best->count<largest)precise_budget_stats[0]++;return best;}
 if(!install)return NULL;
 if(largest)precise_budget_stats[1]++;
 for(unsigned w=0;w<ways;w++)if(!precise_dispatch[set+w*stride].fn)return &precise_cache[set+w*stride];
 unsigned w=precise_victim[set]++%ways;
 return &precise_cache[set+w*stride];
}

static void ee_precise_bump(uint32_t *generation)
{
 if(++*generation==0u)*generation=1u;
}
static uint32_t ee_precise_next_serial(void)
{
 ee_precise_bump(&precise_serial_source);
 return precise_serial_source;
}
static void ee_precise_publish_dispatch(const ee_precise_slot *slot)
{
 unsigned i=(unsigned)(slot-precise_cache);
 precise_dispatch[i]=(ee_precise_dispatch){slot->pc,slot->count,slot->source_page,
  slot->source_generation,slot->mapping_generation,slot->serial,slot->words[0],slot->fn};
}
/* R1318: publish a replacement only after translation/finalization succeeded.
 * A live chain pins all installed allocations via precise_active, so failed or
 * nested installs leave the old slot byte-for-byte intact. */
static int ee_precise_install_slot(ee_precise_slot *slot,uint32_t pc,
 const uint32_t *words,unsigned count,uint32_t source_page,
 uint32_t source_generation,uint32_t mapping_generation,ee_precise_fn fn)
{
 if(!slot||!words||!fn||count<2u||count>8u||precise_active)return 0;
 ee_precise_slot next;memset(&next,0,sizeof(next));
 next.pc=pc;next.count=count;memcpy(next.words,words,count*4u);
 next.source_page=source_page;next.source_generation=source_generation;
 next.mapping_generation=mapping_generation;next.serial=ee_precise_next_serial();
 next.fn=fn;
 if(gekko2_opt_enabled(GEKKO2_OPT_CACHE_REUSE)) {
  unsigned set=ee_precise_cache_index(pc),stride=ee_precise_cache_stride();
  for(unsigned w=0;w<4u;w++) {
   ee_precise_slot *other=&precise_cache[set+w*stride];
   if(other!=slot&&other->fn&&other->pc==pc&&other->count!=count){precise_budget_stats[2]++;break;}
  }
 }
 ee_precise_fn old=slot->fn;
 int displaced=old&&(slot->pc!=pc||slot->count!=count||
                    memcmp(slot->words,words,count*4u)!=0);
 *slot=next;
 ee_precise_publish_dispatch(slot);
 if(old&&old!=fn){if(displaced)precise_evictions++;ppc_code_cache_release((void*)old);}
 return 1;
}
static void ee_precise_release_slot(ee_precise_slot *slot)
{
 if(!slot||precise_active)return;
 if(slot->fn)ppc_code_cache_release((void*)slot->fn);
 memset(&precise_dispatch[slot-precise_cache],0,sizeof(precise_dispatch[0]));
 memset(slot,0,sizeof(*slot));
}
void ee_jit_notify_physical_write(uint32_t phys_addr,uint32_t len)
{
 if(!len||phys_addr>=32u*1024u*1024u)return;
 uint64_t last=(uint64_t)phys_addr+(uint64_t)len-1u;
 if(last>=32u*1024u*1024u)last=32u*1024u*1024u-1u;
 uint32_t first=phys_addr>>12,end=(uint32_t)last>>12;
 for(uint32_t page=first;page<=end;page++)ee_precise_bump(&precise_page_generation[page]);
}
extern void ee_fastmem_invalidate(void) __attribute__((weak));
void ee_jit_notify_mapping_change(void)
{
 if(ee_fastmem_invalidate)ee_fastmem_invalidate();
 ee_precise_bump(&precise_mapping_generation);
}
uint64_t ee_jit_get_block_count(void){return precise_runs;}
uint64_t ee_jit_get_block_retired(void){return precise_retired;}
uint64_t ee_jit_get_native_retired_count(void){return g_jit_executed+precise_retired;}
uint64_t ee_jit_get_block_evictions(void){return precise_evictions;}
uint64_t ee_jit_get_direct_link_hits(void){return precise_direct_link_hits;}
typedef unsigned (*ee_cached_chain_fn)(ee_state_t *,unsigned,uint32_t,unsigned,ee_precise_fn,unsigned);
static ee_cached_chain_fn precise_chain_fn;
static uint64_t precise_native_successors;
uint64_t ee_jit_get_native_successors(void){return precise_native_successors;}
#if GEKKO2_EE_BLOCKS_ENABLED
static int ee_precise_source_snapshot(ee_state_t *st,uint32_t pc,uint32_t *page,uint32_t *generation)
{
 if(!ee_core_block_source_page(st,pc,page))return 0;
 if(*page==EE_PRECISE_NON_RAM_PAGE){*generation=0u;return 1;}
 if(*page>=EE_PRECISE_RAM_PAGES)return 0;
 *generation=precise_page_generation[*page];
 return 1;
}
static int ee_precise_slot_generation_current(const ee_precise_slot *slot)
{
 if(slot->mapping_generation!=precise_mapping_generation)return 0;
 return slot->source_page==EE_PRECISE_NON_RAM_PAGE ||
        (slot->source_page<EE_PRECISE_RAM_PAGES &&
         slot->source_generation==precise_page_generation[slot->source_page]);
}
/* No allocation, eviction or compilation here. precise_active pins all
 * precise-cache allocations until the complete native chain returns. */
static uint64_t ee_precise_cached_next(ee_state_t *st,unsigned remaining)
{
 if(precise_active&&ee_core_interleave_quantum!=UINT32_MAX) {
  ee_core_interleave_account(precise_accounted_remaining-remaining);
  precise_accounted_remaining=remaining;
 }
 remaining=ee_core_interleave_limit(remaining);
 if(!precise_active||!st||remaining<2u||st->halted||st->idle||st->branch_pending||st->next_pc!=st->pc+4u)return 0;
 ee_precise_slot *source=precise_chain_source,*slot=0;
 ee_precise_slot *found=ee_precise_find(st->pc,0,remaining);
 if(!found)return 0;
 unsigned index=(unsigned)(found-precise_cache);
 const ee_precise_dispatch *hot=&precise_dispatch[index];
 if(hot->fn&&hot->pc==st->pc&&hot->count>=2u&&hot->count<=remaining&&
    hot->mapping==precise_mapping_generation&&
    (hot->page==EE_PRECISE_NON_RAM_PAGE||
     (hot->page<EE_PRECISE_RAM_PAGES&&hot->generation==precise_page_generation[hot->page]))) {
  uint32_t live;
  if(!ee_core_block_peek(st,st->pc,&live)||live!=hot->word)return 0;
  /* precise_active excludes replacement/release. Live per-instruction
   * prepares still check every subsequent word and memory proof. */
  if(source&&source->serial==precise_chain_source_serial&&
     source->link_pc==hot->pc&&source->link_serial==hot->serial)precise_direct_link_hits++;
  if(source&&source->serial==precise_chain_source_serial) {
   source->link_pc=hot->pc;source->link_serial=hot->serial;
   source->link_index=(uint16_t)index;source->link_fn=hot->fn;
  }
  precise_chain_source=&precise_cache[index];precise_chain_source_serial=hot->serial;
  precise_dispatch_hits++;precise_native_successors++;
  return ((uint64_t)(uint32_t)(uintptr_t)hot->fn<<32)|hot->count;
 }
 /* Fast edge: use the predecessor's learned direct successor when every
  * identity field still matches.  No allocation or compilation can occur
  * while precise_active pins the chain, so the pointer remains owned. */
 if(source&&source->serial==precise_chain_source_serial&&source->link_fn&&source->link_pc==st->pc) {
  unsigned li=source->link_index;
  if(li<EE_PRECISE_CACHE_SLOTS) {
   ee_precise_slot *linked=&precise_cache[li];
   if(linked->serial==source->link_serial&&linked->fn==source->link_fn&&linked->pc==st->pc)
    slot=linked;
  }
 }
 if(!slot)slot=found;
 if(!slot->fn||!slot->serial||slot->pc!=st->pc||slot->count<2u||slot->count>remaining)return 0;
 uint32_t serial=slot->serial;ee_precise_fn fn=slot->fn;
 if(!ee_precise_slot_generation_current(slot))return 0;
 uint32_t word;
 if(!ee_core_block_peek(st,st->pc,&word)||word!=slot->words[0])return 0;
 if(slot->serial!=serial||slot->fn!=fn||slot->pc!=st->pc)return 0;
 if(source&&source->serial==precise_chain_source_serial) {
  unsigned idx=(unsigned)(slot-precise_cache);
  if(source->link_fn==fn&&source->link_pc==slot->pc&&source->link_serial==serial&&source->link_index==idx)
   precise_direct_link_hits++;
  else {
   source->link_pc=slot->pc;source->link_serial=serial;source->link_index=(uint16_t)idx;source->link_fn=fn;
  }
 }
 precise_chain_source=slot;precise_chain_source_serial=serial;
 precise_native_successors++;
 return ((uint64_t)(uint32_t)(uintptr_t)fn<<32)|slot->count;
}
static void ee_precise_make_chain(void)
{
 if(precise_chain_fn)return;
 ppc_codegen_ctx_t c;if(ppc_dynarec_init(&c,1u))return;
 if(ppc_dynarec_translate_ee_cached_chain(&c,(uint32_t)(uintptr_t)ee_precise_cached_next)) {
  ppc_dynarec_free(&c);return;
 }
 precise_chain_fn=(ee_cached_chain_fn)ppc_dynarec_finalize(&c);
 if(!precise_chain_fn)ppc_dynarec_free(&c);
}
#endif
static ee_precise_fn ee_precise_build_impl(uint32_t pc,const uint32_t *words,unsigned count)
{
 ppc_codegen_ctx_t c;if(ppc_dynarec_init(&c,count*2u))return 0;
#ifdef GEKKO2_LEGACY_BOUNDARIES
  int translation=ppc_dynarec_translate_ee_resident_delay_block(&c,pc,words,count,
    (uint32_t)(uintptr_t)ee_core_block_prepare,(uint32_t)(uintptr_t)ee_core_block_prepare_memory_resolved,(uint32_t)(uintptr_t)ee_core_block_prepare_delay,(uint32_t)(uintptr_t)ee_core_block_commit,(uint32_t)offsetof(ee_state_t,gpr_generation));
#else
  int translation=ppc_dynarec_translate_ee_fused_delay_block(&c,pc,words,count,
    (uint32_t)(uintptr_t)ee_core_block_prepare,(uint32_t)(uintptr_t)ee_core_block_prepare_memory_resolved,
    (uint32_t)(uintptr_t)ee_core_block_prepare_delay,(uint32_t)(uintptr_t)ee_core_block_commit,
    (uint32_t)(uintptr_t)ee_core_block_boundary,(uint32_t)(uintptr_t)ee_core_block_memory_boundary,
    (uint32_t)(uintptr_t)ee_core_block_delay_boundary,(uint32_t)offsetof(ee_state_t,gpr_generation));
#endif
  if(translation) {
   ppc_dynarec_free(&c);return 0;
  }
  ee_precise_fn fn=(ee_precise_fn)ppc_dynarec_finalize(&c);
  if(!fn){ppc_dynarec_free(&c);return 0;}
 return fn;
}
static ee_precise_fn ee_precise_build(uint32_t pc,const uint32_t *words,unsigned count)
{
 uint64_t begin=jit_compile_begin(&precise_profile);unsigned old=gp_enter(GP_COMPILE);
 ee_precise_fn fn=ee_precise_build_impl(pc,words,count);
 gp_leave(old);jit_compile_end(&precise_profile,begin);
 if(!fn)precise_profile.failures++;else precise_profile.installed++;
 return fn;
}
static unsigned ee_precise_execute(ee_state_t *st,unsigned budget,unsigned fetched,uint32_t first_word,int native_chain)
{
#if GEKKO2_EE_BLOCKS_ENABLED
 if(!st||budget<2u||st->halted||st->idle||st->branch_pending||precise_active)return 0;
 if(!gekko2_opt_enabled(GEKKO2_OPT_EE_JIT)||!gekko2_opt_enabled(GEKKO2_OPT_EE_BLOCKS))return 0;
 native_chain &= gekko2_opt_enabled(GEKKO2_OPT_NATIVE_LINKS);
 unsigned quantum_budget=ee_core_interleave_limit(budget);
 if(quantum_budget<2u)return 0;
 uint32_t pc=st->pc,words[8];unsigned count=0,limit=quantum_budget<8u?quantum_budget:8u;
 ee_precise_slot *slot=ee_precise_find(pc,1,quantum_budget);
 precise_profile.lookups++;
 /* The emitted prepare callback validates live mapping/encoding before
  * EACH instruction. A warm entry needs no duplicate full-block scan. */
 if(slot->fn&&slot->pc==pc&&slot->count<=quantum_budget&&
    ee_precise_slot_generation_current(slot)&&(!fetched||slot->words[0]==first_word)){precise_profile.hits++;goto execute_slot;}
 precise_profile.misses++;
 if(slot->fn){if(slot->pc!=pc)precise_profile.collisions++;else precise_profile.stale++;}
 limit=ee_core_block_words(st,pc,words,limit);
 for(;count<limit;count++) {
  if(ee_jit_block_terminal(words[count])) {
   count++;
   if(count<limit&&ee_jit_block_candidate(words[count]))count++;
   break;
  }
  if(!ee_jit_block_candidate(words[count]))break;
 }
 if(count<2u)return 0;
 uint32_t source_page,source_generation;
 if(!ee_precise_source_snapshot(st,pc,&source_page,&source_generation))return 0;
 uint32_t mapping_generation=precise_mapping_generation;
 if(!slot->fn||slot->pc!=pc||slot->count!=count||memcmp(slot->words,words,count*4u)) {
  ee_precise_fn fn=ee_precise_build(pc,words,count);if(!fn)return 0;
  if(!ee_precise_install_slot(slot,pc,words,count,source_page,source_generation,mapping_generation,fn)) {
   ppc_code_cache_release((void*)fn);return 0;
  }
 } else {
  /* A stale epoch with byte-identical code reuses the compiled PPC safely;
   * only the validated source/mapping stamp changes; its allocation identity
   * remains stable because no executable buffer was replaced. */
  slot->source_page=source_page;slot->source_generation=source_generation;
  slot->mapping_generation=mapping_generation;
  ee_precise_publish_dispatch(slot);
 }
execute_slot:;
 uint32_t first_physical=0;
 if(fetched) {
  if(slot->words[0]!=first_word)return 0;
  /* The scalar fetch already happened, but preparation must wait until
   * a first memory operation proves its current direct-RAM address. */
  if(ee_jit_block_memory_width(first_word)) {
   first_physical=ee_core_block_memory_resolve(st,first_word);
   if(!first_physical)return 0;
  }
  if(!ee_core_block_prepare_fetched(st,pc))return 0;
 }
 if(native_chain&&slot->count+2u<=budget)ee_precise_make_chain();
 int use_chain=native_chain&&precise_chain_fn&&slot->count+2u<=budget;
 precise_chain_source=use_chain?slot:0;
 precise_chain_source_serial=use_chain?slot->serial:0;
 precise_active=1;
 precise_accounted_remaining=budget;
 unsigned n=use_chain?
  precise_chain_fn(st,fetched,first_physical,budget,slot->fn,slot->count):
  slot->fn(st,fetched,first_physical);
 if(ee_core_interleave_quantum!=UINT32_MAX)ee_core_interleave_account(precise_accounted_remaining-(budget-n));
 precise_active=0;
 precise_chain_source=0;precise_chain_source_serial=0;
 precise_runs++;precise_retired+=n;
 /* Changed first word/mapping: release only after the native function
  * returns. The scalar path handles this instruction; later visits retry. */
 if(!n)ee_precise_release_slot(slot);
 return n;
#else
 (void)st;(void)budget;(void)fetched;(void)first_word;(void)native_chain;return 0;
#endif
}

unsigned ee_jit_try_execute_block(ee_state_t *st,unsigned budget)
{return ee_precise_execute(st,budget,0u,0u,0);}
unsigned ee_jit_try_execute_block_fetched(ee_state_t *st,unsigned budget,uint32_t first_word)
{return ee_precise_execute(st,budget,1u,first_word,0);}

/* Guarded continuation between returned native blocks within the SAME EE
 * budget. No native return address points into an evicted code buffer, and
 * no EE/IOP scheduling boundary is crossed. Each successor keeps live source
 * and data checks; partial exits return to the full scalar frontend. */
unsigned ee_jit_try_execute_chain_fetched(ee_state_t *st,unsigned budget,uint32_t first_word)
{
#if GEKKO2_EE_BLOCKS_ENABLED
 if(!st)return 0;
 uint32_t start=st->pc;
 unsigned n=ee_precise_execute(st,budget,1u,first_word,1),total=n;
 while(n&&ee_core_interleave_limit(budget-total)>=2u) {
  ee_precise_slot *previous=ee_precise_find(start,0,n);
  if(!previous||previous->pc!=start||n!=previous->count||st->halted||st->idle||st->branch_pending||st->next_pc!=st->pc+4u)break;
  uint32_t instruction;
  if(!ee_core_block_peek(st,st->pc,&instruction)||
     (!ee_jit_block_candidate(instruction)&&!ee_jit_block_terminal(instruction)))break;
  start=st->pc;
  n=ee_precise_execute(st,budget-total,0u,0u,1);
  total+=n;
 }
 return total;
#else
 (void)st;(void)budget;(void)first_word;return 0;
#endif
}

static void ee_precise_reset_cache(void)
{
 if(precise_active)return; /* Never release the currently executing buffer. */
 if(precise_chain_fn)ppc_code_cache_release((void*)precise_chain_fn);
 precise_chain_fn=0;precise_native_successors=0;precise_direct_link_hits=0;
 precise_chain_source=0;precise_chain_source_serial=0;
 /* Empty entries need no individual clears; both arrays are cleared below. */
 for(unsigned n=0;n<EE_PRECISE_CACHE_SLOTS;n++)
  if(precise_cache[n].fn)ee_precise_release_slot(&precise_cache[n]);
 memset(precise_cache,0,sizeof(precise_cache));
 memset(precise_dispatch,0,sizeof(precise_dispatch));precise_dispatch_hits=0;
 memset(precise_page_generation,0,sizeof(precise_page_generation));
 precise_mapping_generation=0;precise_serial_source=0;
 precise_runs=precise_retired=precise_evictions=0;
 memset(precise_victim,0,sizeof precise_victim);memset(&precise_profile,0,sizeof precise_profile);
 memset(precise_budget_stats,0,sizeof precise_budget_stats);
}
