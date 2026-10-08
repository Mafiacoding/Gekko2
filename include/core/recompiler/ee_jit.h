#ifndef PCSX2WII_EE_JIT_H
#define PCSX2WII_EE_JIT_H

/* R1281: native MFC0/MTC0 transfers; single-op CPU dispatch uses inline C.
 * Full EE/IOP block JIT remains unfinished. */
#include <stdint.h>
#include "core/recompiler/ee_block_policy.h"
#include "core/ee/ee_core.h"

/* Current R1267 design: one-instruction PPC dispatch with instruction-keyed
 * owning cache and PC+word-tagged L0. Both supported functions and known
 * rejections are cached; allocation/finalization failures are retried.
 * Loads, branches, COP1, VU0 macro operations and many MMI instructions are
 * implemented. The interpreter's instruction epilogue remains mandatory.
 * Full EE/IOP blocks remain unfinished; IOP single-op and VU pair backends
 * are implemented with partial opcode/timing coverage.
 * The older round-by-round design notes below describe historical scope. */

/*
 * ee_jit - Round 887 (task #866/#868 continuation): the first real
 * wiring of the ppc_dynarec.c PoC (source/core/recompiler/ppc_dynarec.c)
 * into actual EE execution (ee_core.c's ee_step()).
 *
 * DESIGN / SAFETY MODEL (read before touching the opcode set below):
 *
 * ee_step() does far more per instruction than decode-and-execute one
 * MIPS opcode - its ~60-line epilogue (ee_core.c, right after the
 * giant opcode switch) latches/checks timer and INTC/DMAC interrupts,
 * ticks VBLANK and the EE peripheral timers, and runs half a dozen
 * project-specific HLE heuristics (boot-unblock guards, RPC/CDVD
 * pending checks, browser-menu escalation, etc.) - all deliberately
 * calibrated, across hundreds of prior rounds, to fire at EVERY
 * genuine instruction boundary (see e.g. Round 598's regression, which
 * found that even moving ONE of these checks to a coarser granularity
 * broke real boot progress). A JIT that batched multiple instructions
 * per native call and only ran that epilogue once per BATCH - the
 * "obvious" JIT design - would silently change that cadence and risk
 * exactly the class of regression this project has spent enormous
 * effort finding and fixing before.
 *
 * So this JIT does NOT batch instructions and does NOT skip the
 * epilogue. It intercepts exactly ONE thing: for a small, fixed set of
 * pure-ALU MIPS opcodes (no memory access, no branching, no exception-
 * raising possibility - see ee_jit_try_execute_one()'s implementation
 * for the authoritative list, kept in sync with
 * ppc_dynarec_translate_one()'s own supported set), the REGISTER
 * COMPUTATION for that single instruction is done by calling real,
 * cached, natively-executing PPC750 machine code instead of running
 * the interpreter's C switch case - operating directly on
 * `&st->gpr[0]` (zero-copy, exactly the integration ppc_dynarec.h's
 * own header comment described as the eventual goal). Every other
 * part of ee_step() (delay-slot bookkeeping, the epilogue, exception
 * context, $zero handling, instructions_executed accounting) runs
 * completely unchanged, at the same per-instruction granularity as
 * before this file existed.
 *
 * Each of the initial 11 supported opcodes' JIT semantics were
 * verified bit-for-bit against ee_core.c's OWN interpreter case bodies
 * (not just an independently-derived MIPS ISA model) before this file
 * was wired in - see docs/STATUS.md Round 887 for the full comparison.
 * The supported set has grown substantially since (branches, loads/
 * stores, COP1 FPU, COP2/VU0, and the full MMI family - see
 * ppc_dynarec.h's own round-by-round log for each), so "11" above is
 * now historical, not current; ee_jit_opcode_supported() in
 * ee_jit.c is the authoritative current list.
 *
 * Round 922 (task #913): ee_jit_opcode_supported() - the pre-filter
 * that gates whether ee_jit_try_execute_one() even ATTEMPTS the JIT
 * path for a given instruction - had silently fallen out of sync with
 * ppc_dynarec_translate_one()'s real dispatch: it was last updated at
 * Round 907 and only ever recognized VADD/VSUB/VMUL under op==0x12,
 * and had ZERO entries for op==0x1Cu (MMI) at all. This meant every
 * COP2/VU0 opcode from Rounds 908-913 and every MMI opcode from Rounds
 * 914-921 - a huge fraction of this dynarec's total opcode coverage -
 * was being correctly compiled by translate_one() but was NEVER
 * actually reached on real GEKKO hardware, silently falling back to
 * the interpreter for every single one of those instructions a real
 * game/BIOS executes. Fixed by widening the pre-filter to a blanket
 * "op==0x12 or op==0x1Cu is supported" match rather than re-deriving
 * the exact per-opcode enumeration a second time in a second place
 * (the same hand-sync burden that caused the staleness in the first
 * place) - safe because ee_jit_try_execute_one() already treats a
 * nonzero ppc_dynarec_translate_one() return as "fall back to the
 * interpreter" for every opcode, so any sub-opcode within those two
 * families that translate_one() doesn't yet implement (the
 * intentionally-deferred MADD/MADDU/MADD1/MADDU1/MULT1/MULTU1/DIV1/
 * DIVU1/PLZCW/QFSRV) just costs one wasted cheap compile attempt, not
 * a correctness risk. Verified via r922_gate_verify.c (13/13 checks,
 * 0 ASan/UBSan errors): confirms the gate now recognizes real Round
 * 908-921 encodings it previously rejected, confirms it still accepts
 * every opcode it recognized before, and confirms translate_one()
 * genuinely still returns -1 (not garbage or a crash) for each of the
 * 3 deferred opcodes sampled, so the fallback path this whole
 * argument rests on is empirically true, not just asserted.
 *
 * Because none of the supported opcodes read memory or depend on PC,
 * a single compiled block is valid for every future occurrence of the
 * exact same 32-bit instruction WORD, anywhere in memory - so the
 * cache below is keyed by instruction encoding, not by address. This
 * sidesteps self-modifying-code invalidation entirely for this opcode
 * class (there is nothing address-dependent to invalidate).
 */

/* Attempts to execute `instr` (the raw MIPS word already fetched at
 * st->pc, exactly as ee_step() decodes it) via the PPC dynarec.
 * Returns 1 if it was executed this way (the caller must skip its own
 * interpreter case for this instruction and fall through to the
 * epilogue), 0 if this opcode isn't currently JIT-supported (caller
 * should run its normal interpreter switch, exactly as before). */
int ee_jit_try_execute_one(ee_state_t *st, uint32_t instr);

/* R1162: PC-tagged hot trace front-end.  Same one-instruction semantics as
 * ee_jit_try_execute_one(), but lets the JIT remember the already-resolved
 * native function for a hot instruction address.  The raw instruction word
 * is still supplied and compared on every hit, so self-modifying code or an
 * overlay change invalidates the entry automatically. */
int ee_jit_try_execute_one_at(ee_state_t *st, uint32_t pc, uint32_t instr);

/* Diagnostics for verification/STATUS.md writeups and host-native
 * tests - not used by any control-flow decision. */
uint64_t ee_jit_get_executed_count(void);
/* All retired native EE instructions: scalar stubs plus precise blocks. */
uint64_t ee_jit_get_native_retired_count(void);
/* R1267: negative L0 hits avoid repeated unsupported-opcode compilation. */
uint64_t ee_jit_get_rejected_hit_count(void);
uint64_t ee_jit_get_compile_attempt_count(void);
uint32_t ee_jit_get_cache_size(void);
uint64_t ee_jit_get_pc_l0_hit_count(void);
uint64_t ee_jit_get_pc_l0_miss_count(void);
/* R1175: runtime C-helper profiler. Diagnostic only. */
extern volatile uint64_t g_r1175_jit_sqrt_calls;
extern volatile uint64_t g_r1175_jit_cvt_calls;
extern volatile uint64_t g_r1175_jit_mmi_muldiv_calls;
extern volatile uint64_t g_r1175_jit_pmfhl_calls;
void     ee_jit_reset_stats_for_test(void); /* test-only: zero counters + cache, so successive host-native tests don't see stale state from an earlier test in the same process */

unsigned ee_jit_try_execute_block(ee_state_t *st,unsigned budget);
uint64_t ee_jit_get_native_successors(void);
uint64_t ee_jit_get_block_count(void);
uint64_t ee_jit_get_block_retired(void);
/* R1318: number of valid precise blocks displaced by direct-map collisions. */
uint64_t ee_jit_get_block_evictions(void);
/* R1326: warm serial/generation-safe direct successor-link hits. */
uint64_t ee_jit_get_direct_link_hits(void);
uint64_t ee_jit_get_dispatch_hits(void);

/* R1316: lazy precise-block invalidation. CPU/DMA RAM writers bump the
 * physical source-page generation; mapping mutations bump a separate epoch. */
void ee_jit_notify_physical_write(uint32_t phys_addr,uint32_t len);
void ee_jit_notify_mapping_change(void);

/* First encoding came from the real scalar fetch; later words stay live. */
unsigned ee_jit_try_execute_chain_fetched(ee_state_t *st,unsigned budget,uint32_t first_word);
unsigned ee_jit_try_execute_block_fetched(ee_state_t *st,unsigned budget,uint32_t first_word);

#endif
