# PS2Recomp strict parser — Batch A15: PPC cross-build check and word loads/stores

Date: 2026-11-08 · Branch `codex/ps2recomp-bridge-batch-a-memory` (PR #58, Draft)
Pinned upstream: `e5636981a1c9de368f6b71e819c3dc188dfedbf4`

## A15a — ELF-bound AOT built for the Wii and checked as real PPC code

- `tools/build_wii_private_aot.sh PRIVATE_DIR` takes the directory written by
  `ps2recomp_strict_parser.py --elf … --output-dir …`, copies the sources to a
  scratch tree, drops the private `ee_aot_generated.inc` /
  `ee_aot_complex_generated.inc` in, cross-builds with devkitPPC (the R1389
  AOT/DSI flag set), runs `verify_wii_memory.py` and writes the ELF back into the
  private directory. The tracked tree is never modified; a directory inside the
  checkout is refused.
- `tools/verify_wii_ps2recomp_aot.py` reuses the audited linked-PPC fixture
  (`ee_ppc_fixture_internal.py`) and runs the **real Wii ELF** under the Unicorn
  PowerPC model: for each function, interpreter (optimizations off) versus the
  generated AOT block from the same randomised 128-bit GPR state, several budget
  splits; the complete `ee_state_t` plus the code window and the data area must
  be byte-identical, the native-retired counter must advance, and a rewritten
  guest word must make AOT fall back to the interpreter.
- `tools/ps2recomp_wii_ppc_check.py` runs the whole chain (parse → bind → private
  dir → cross-build → PPC differential) for every committed fixture; the
  `wii-build.yml` workflow runs it.
- Local results (devkitPPC r51, GCC 16.2.0, libogc 3.2.1, Unicorn 2.1.4): A11
  98, A12 147, A13 210, A15 156 PPC comparisons, all PASS, with 360 / 768 / 408 /
  396 natively retired instructions. Altering the generated jump target
  (`next_pc=target+4`) makes the PPC check fail.
- This is **modelled** PowerPC (Unicorn), not a Wii or Dolphin. No speed claim.

## A15b — LW and SW (word, register base) in the parser

New real-generator fixture `tests/fixtures/ps2recomp_a15/` (5 functions, 0
unhandled). Accepted exactly:

- `lw`: `SET_GPR_S32(ctx, rt, (int32_t)READ32(ADD32(GPR_U32(ctx, rs), simm)));`
- `sw`: `WRITE32(ADD32(GPR_U32(ctx, rs), simm), GPR_U32(ctx, rt));`

(`simm` printed as unsigned 32-bit decimal; `lw` to `$zero` still emits the
setter and is accepted; both work in a `jr $ra` delay slot.)

**Real finding — constant folding changes the output.** When the base register
is a compile-time constant earlier in the same function (`lui t0,0x8030; lw
t1,0(t0)`), upstream emits `FAST_READ32(0x80300000u)` and a
`do { … ps2TraceGuestWrite(…); FAST_WRITE32(…); } while (0)` store (and
`runtime->Load32/Store32` for MMIO/special addresses). The parser recognises
only the unhinted form, so such functions are **rejected** (fixture
`a15_hinted`, kept as a real negative test). Supporting them would require
reproducing upstream's constant tracking; not done.

Semantics come from Gekko2 (`ee_mem_read32/write32`, TLB, MMIO guards) via the
raw word; the C++ is only a cross-check that the word is what the generator saw.
Other widths (lb/lh/lbu/lhu/lwu/ld/sb/sh/sd/lq/sq/unaligned/FPU/VU) remain
rejected.

### Evidence (host-native)

- 55 parser tests OK (new: memory template variants, 19 other memory opcodes,
  exhaustive number mutation of the 4 memory functions, ELF binding, delay-slot
  memory ops, constant-folded form rejected).
- Differential (host core): A15 480 comparisons (312 partial budget) source-only
  and ELF-bound, plus 72 fault/device cases and 12 live-source cases. Valid
  accesses compare the full state and a 128 KiB data area; unaligned, MMIO
  (`0x10003000`) and unmapped-KUSEG accesses must give identical exception state,
  retirement and RAM in both modes (the unmapped case is asserted to raise a TLB
  exception). Altering a generated store offset fails the test.
- **Observation (not changed):** Gekko2's EE core raises no address error for an
  unaligned word load/store — both paths agree, but the real R5900 would trap.
  Out of scope for this batch.
- Not tested: physical Wii/Dolphin, performance.

## Limits / next

Straight-line + terminal `jr $ra`, ≤32 words, entry at function start. Next:
byte/halfword loads/stores (one width at a time), then conditional branches
inside a function, then calls between recompiled functions.
