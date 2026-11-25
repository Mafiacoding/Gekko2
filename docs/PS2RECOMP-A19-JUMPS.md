# PS2Recomp strict parser — Batch A19: `j` inside a function (indirect forms deferred)

Date: 2026-11-08 · Branch `codex/ps2recomp-a19-jumps`, based on `codex/ps2recomp-a18-calls`
(which is now also the head of `integration/crash-e3-plus-a15`: A17/A18 were fast-forwarded in).
Read `docs/BRANCH-MAP-2026-11-08.md` first. Pinned upstream `e5636981…`. No runtime source changed
(`tools/check_crash_files_unchanged.sh` OK).

## Accepted

Real-generator fixture `tests/fixtures/ps2recomp_a19/` (4 functions, 0 warnings/errors):

- `j target` to a target **inside the same function**, forward or backward, with a real or elided-NOP
  delay slot. Exact text: `ctx->pc = J;`, optional slot block, `ctx->pc = TARGET;`, for backward jumps
  `if (runtime->eeCheckpointDue()) { return; }`, `goto label_TARGET;`, `ctx->pc = J+8;` (uppercase hex).
- The same label rules as branches: labels exactly at targets, and a backward target other than the
  function start must appear in the resume switch (observed: `a19_jback2`). `j` is recorded in the
  branch list (`kind: "j"`), so loops closed by `j` and conditional exits combine freely.
- Lowering: `j`+slot ends an AOT block; the unreachable code after it keeps a block entry that is never
  executed natively (harmless, tested).

## Rejected / deferred (real upstream output)

- `j` to another function: upstream emits a C++ call + `return` (A18 fixture `a18_tail`); rejected as before.
- **`jr` through a register other than `$ra`, and `jalr` mid-function**: upstream wraps them in heavy
  scaffolding (`switch` with a case for every instruction address, `label_…` pairs,
  `if (ctx->pc == …) goto label_fallthrough_…`, `GuestBranchKind::IndirectJump/IndirectCall`).
  The shapes were captured but are **not parsed**: they are irregular and one combined generator run
  with them ended in an upstream internal error (*combined output completion queue is missing index 4*),
  so they are not committed as fixtures. A20 would need to fix a precise grammar first.

## Evidence (host and modelled PPC only)

- Parser: **84 tests OK** (new: golden jumps/blocks/labels/resume table, checkpoint-poll rules,
  tail-jump rejection, every-line deletion/insertion and exhaustive number mutation of all four
  functions, ELF binding, lowering equals `generate(blocks)`).
- Host differential (source-only and ELF-bound): **480** comparisons per mode, loops run once or leave at
  once (t2 = t1+1/+2), dynamic retirement up to 11, 384 partial-budget, 12 live-source. All earlier
  fixtures (A11–A18) re-run in the same harness and pass. Altering a generated jump target aborts the run.
- Linked PPC (Unicorn, devkitPPC r51 ELF): see the A19 row in the final table of this batch's report
  (the check covers every fixture A11–A19).

## Limits

Same as A17/A18; no indirect jumps/calls other than the final `jr $ra`, no branch-likely. No Dolphin or Wii run.
