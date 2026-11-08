# PS2Recomp strict parser — Batch A13: `jr $ra` with delay slot

Date: 2026-11-08 · Branch `codex/ps2recomp-bridge-batch-a-memory` (PR #58, Draft)
Pinned upstream: `e5636981a1c9de368f6b71e819c3dc188dfedbf4`

## Result

Functions may now end in `jr $ra` (word exactly `0x03e00008`) plus its delay
slot, which is what real leaf functions look like. Only this single terminal
form is accepted: no other jump, call, branch, label or internal target.

## Real upstream output (new fixture `tests/fixtures/ps2recomp_a13/`)

Six functions from the unmodified pinned generator (0 unhandled, 0 warnings).
Findings that only the real binary shows:

- Branch blocks print hex **uppercase** (`ctx->pc = 0x10001Cu;`, `branch_pc`,
  the `dispatchGuestBranch` argument and the trailing fallthrough
  `ctx->pc = 0x100024u;`), straight-line code prints lowercase. The parser
  reconstructs each form exactly (`test_uppercase_hex_only_after_control_flow`,
  fixture `a13_plain_hex` has a lowercase hex end `0x10002c`).
- A **NOP delay slot is elided**: the source has the `jr` block with no slot
  statements (`isGuestNop` = `sll $zero,$zero,0` with any rs bits, or
  `addiu $zero,$zero,0`). Fixture `a13_ret_nop` and `a13_nop_alias_slot`.
- Non-NOP slots appear as `ctx->pc = slot; ctx->in_delay_slot = true;
  ctx->branch_pc = jr; // … (Delay Slot); <statement>;
  ctx->in_delay_slot = false;`. The jump target is read **before** the slot
  (`a13_ra_slot` clobbers `$ra` in the slot; MIPS and Gekko2's JR agree).
- The function-closing brace is column 0; the `jr` block's own `}` is
  indented. The parser now ends a function only at a column-0 `}`.
- The block contains `#if defined(PS2X_STRICT_RETURN_DIAGNOSTICS)` lines. They
  are accepted only as part of the exact reconstructed block; both branches set
  `ctx->pc = jumpTarget` and return, so the diagnostics `dispatchGuestBranch`
  call does not change guest state and is never executed.

## Parser rules added

- `jr $ra` must be second to last: range end = jr address + 8.
- Head/tail lines are compared to the reconstructed upstream text; a present
  slot must be a supported non-NOP instruction (a decoded NOP slot is
  inconsistent and rejected); mnemonics of both comments must agree.
- Total words including the slot ≤ 32.
- **Inferred slot**: when the slot is elided the lowered word is `0`
  (`inferred_nop_slot: true` in the result). The source cannot prove which
  guest NOP encoding was elided. This is safe because AOT checks the live
  memory word before every instruction: with a different encoding native code
  stops before the slot and the interpreter retires it
  (`a13_nop_alias_slot`, RAM word `0x24000000`).

## Evidence (host-native)

- Parser tests 39 OK: exhaustive single-token mutation of every number/hex in
  all six real functions (111 mutations, all rejected, interpreted
  disassembly operands excluded), line deletion/insertion in the `jr` block,
  tampered epilogue/prologue/flags, other jumps and branches, range and
  32-word limit boundaries (30 + jr + slot accepted, 31 rejected).
- Differential: 720 parsed-function/interpreter comparisons (504 partial
  budget), 6 functions, 18 live-source guard cases; additionally asserts the
  final PC equals the original `$ra` low 32 bits, no pending branch, upper
  lanes preserved. Manually altering the generated `next_pc` aborts the run.

## Limits

Single-block, straight-line + terminal `jr $ra`; no calls, branches, memory.
Entry only at function start. No Wii/Dolphin result; no performance claim.
