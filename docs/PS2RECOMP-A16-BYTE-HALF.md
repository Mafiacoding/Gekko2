# PS2Recomp strict parser — Batch A16: byte and halfword loads/stores

Date: 2026-11-08 · Branch `codex/ps2recomp-a16-byte-half` (based on A15 `4cd3f3d`; PR #58 untouched)
Pinned upstream: `e5636981a1c9de368f6b71e819c3dc188dfedbf4`
Read first: `docs/CRASH-FIX-PRESERVATION-AND-A15-INTEGRATION.md` (Crash fixes are preserved;
nothing here touches VU, GIF, VIF, GS/GX or checkpoint code).

## What is accepted

Real-generator fixture `tests/fixtures/ps2recomp_a16/` (6 functions, 0 unhandled):

| Instr | Upstream statement (register base, unhinted form) |
|---|---|
| `lb` | `SET_GPR_S32(ctx, rt, (int8_t)READ8(ADD32(GPR_U32(ctx, rs), simm)));` |
| `lbu` | `SET_GPR_ZE32(ctx, rt, (uint8_t)READ8(ADD32(GPR_U32(ctx, rs), simm)));` |
| `lh` | `SET_GPR_S32(ctx, rt, (int16_t)READ16(ADD32(GPR_U32(ctx, rs), simm)));` |
| `lhu` | `SET_GPR_ZE32(ctx, rt, (uint16_t)READ16(ADD32(GPR_U32(ctx, rs), simm)));` |
| `sb` | `WRITE8(ADD32(GPR_U32(ctx, rs), simm), (uint8_t)GPR_U32(ctx, rt));` |
| `sh` | `WRITE16(ADD32(GPR_U32(ctx, rs), simm), (uint16_t)GPR_U32(ctx, rt));` |

Also covered by the fixtures: `rt == $zero` loads, negative/large offsets, `rt == rs`
aliasing, and a byte store / halfword load in the `jr $ra` delay slot. Semantics come from
Gekko2's own `ee_mem_read8/16` / `ee_mem_write8/16` via the existing AOT backend
(no host-pointer casts); the C++ only cross-checks the raw word.

Rejected as before: upstream's constant-folded form (`FAST_READ8(0x80300000u)`,
`do { … FAST_WRITE16(…) } while (0)`, kept as the real negative function `a16_hinted`),
`lwl/lwr/lwu/swl/swr/ld/sd/lq/sq`, FPU/VU memory ops, and every other opcode.

## Tests and results (host, plus modelled PPC)

- Parser: **62 tests OK** (new: golden words, constant-folded forms rejected, width/
  sign/cast confusion, opcode swaps, exhaustive number mutation of all five functions,
  ELF binding, delay-slot memory ops).
- Host differential (AOT vs EE interpreter, source-only and ELF-bound): A16 **600**
  comparisons (432 partial-budget, i.e. exact retirement across budget splits 1/2/3/n−1/n),
  **120 fault/device cases**, 15 live-source cases. Compared: full `ee_state_t` (128-bit
  GPRs, HI/LO, COP0 incl. Cause/BadVAddr/EPC), 128 KiB data area, exact retired count.
  Scenarios: valid; **misaligned by one byte**; **MMIO** (`0x10003000`, AOT must defer);
  **unmapped KUSEG** (asserted to raise a TLB exception); **write-protected mapped page**
  (V without D; asserted to raise TLB Modified for stores — 24 observed).
- Linked PPC (Unicorn model, real devkitPPC r51 ELF with the ELF-bound blocks):
  **229** interpreter/AOT comparisons over 5 functions, including the same fault/device
  modes (4 TLB Modified faults observed); the checks for A11/A12/A13/A15 are unchanged.
- Wii cross-build with devkitPPC r51 / GCC 16.2.0 / libogc 3.2.1: done by
  `tools/ps2recomp_wii_ppc_check.py` for each fixture (and the default build). 

## Observed alignment behaviour (not changed here)

Gekko2's EE core raises **no address-error exception for a misaligned halfword (or word)
access**: 0 AdEL/AdES in all misaligned runs, host and PPC. AOT and interpreter agree, so
lowering is faithful to the current core, but real R5900 hardware traps. This is an
existing core accuracy gap, not an A16 regression, and was deliberately left alone to avoid
changing EE semantics in this batch. Byte accesses have no alignment requirement.

## Limits / not claimed

Straight-line + terminal `jr $ra`, ≤ 32 words, register-base accesses only. No Wii, Dolphin
or hardware run; no performance claim. Not yet re-validated on the integration line
(`ee_mem_*` are rewritten by Crash commit `7791683`); see the preservation document §4.
