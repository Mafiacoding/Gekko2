# PS2Recomp strict parser — Batch A17: conditional branches inside a function

Date: 2026-11-08 · Branch `codex/ps2recomp-a17-branches`, based on `integration/crash-e3-plus-a15`
(Crash E3 runtime + A11–A16). Read `docs/BRANCH-MAP-2026-11-08.md` and
`docs/CRASH-FIX-PRESERVATION-AND-A15-INTEGRATION.md` first. Pinned upstream `e5636981…`.

No runtime source changed: `tools/check_crash_files_unchanged.sh` reports the protected
Crash files (VU, GIF, VIF, GS/GX, checkpoint, AOT/JIT) identical to
`preserve/crash-e3-vu-runtime-20261108`.

## What is accepted

Real-generator fixture `tests/fixtures/ps2recomp_a17/` (6 functions, 0 unhandled):

- `beq`, `bne`, `blez`, `bgtz` (no likely/link/REGIMM forms), forward and backward, to a
  target **inside the same function**, with a real delay slot or with an elided NOP slot.
- Exact upstream shapes, reconstructed per branch: `ctx->pc` (uppercase hex), the
  `{ const bool branch_taken_0x… = (cond); [delay slot block] if (…) { ctx->pc = target;
  [backward: if (runtime->eeCheckpointDue()) { return; }] goto label_…; } }` block and the
  `ctx->pc = <branch+8>` line. Conditions: `GPR_U64 ==/!=` for beq/bne,
  `GPR_S32 <= 0 / > 0` for blez/bgtz.
- `label_<hex>:` lines must sit exactly at branch targets (and only there); the optional
  `switch (ctx->pc) { case …: goto label_…; default: break; }` resume table must list exactly
  the backward-branch targets other than the function start (observed in the real output).
- The function must end in the A13 `jr $ra` block. `beq $zero,$zero,…` (always taken) has the
  same shape and is accepted (fixture `a17_uncond`).

Rejected (tests): branch targets outside the function (real upstream output is
`ctx->pc = 0x100140u; return;`, fixture `a17_external` kept as a negative), targets that are delay
slots, missing/extra/misplaced labels, wrong resume table, any change to a statement, hex case,
`eeCheckpointDue` placement, condition, rt≠0 in blez/bgtz, plus every previously rejected class.

## Lowering

`basic_blocks()` splits the validated flat word list into AOT blocks: entry at the function
start, at every label, and after every branch+slot; a block ends after its terminal
(branch/`jr` + slot) or before the next entry. Each block is lowered through the existing
`tools/ee_aot_generate.py --manifest` as its own entry (so `ee_core_step_n` →
`ee_step_budget` → `ee_aot_try_execute_fetched` keeps owning retirement and interleave
accounting; no scheduler change). Elided NOP slots are inferred as `0` until
`bind_to_elf()` replaces **every** such slot with the real, upstream-`isGuestNop` ELF word.
The private build writes the block list to `functions.private.json`.

## Evidence (host and modelled PPC only)

- Parser: **71 tests OK** (new: golden branches/blocks, external-branch real negative, ELF
  binding of all elided slots, every-line deletion/insertion, exhaustive number mutation of all
  accepted functions, semantic tamper cases, unsupported branch words, lowering equals
  `generate(blocks)`).
- Host differential (source-only and ELF-bound): A17 **600** comparisons per mode with a
  *dynamic* retirement count (loops, up to 26 instructions per run), budget splits 1/2/3/n−1/n
  (480 partial), 15 live-source cases; GPR0, upper lanes, complete `ee_state_t`, exact retirement,
  exit PC. A17 uses a probe run of the interpreter to learn the retirement count first.
  Altering a generated branch target aborts the run.
- Linked PPC (Unicorn, devkitPPC r51 ELF with the ELF-bound blocks): **245** comparisons,
  1897 natively retired instructions; A11–A16 results unchanged.

## Upstream quirk (informational)

Upstream evaluates `blez`/`bgtz` on `GPR_S32` (low 32 bits); the R5900 and Gekko2 compare the
full 64-bit register. Lowering follows Gekko2's raw-word semantics, so generated C++ and the
Gekko2 result can differ for values whose upper word disagrees with the low word. The parser
only cross-checks the text; this is not hidden.

## Limits

≤ 32 words per function, ≤ 32 per block, no calls between functions, no branch-likely, no
indirect jumps except the final `jr $ra`, entries only at function/label/fall-through PCs. No
Dolphin or Wii run; no performance claim.
