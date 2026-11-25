# PS2Recomp strict parser — Batch A18: direct calls (`jal`) between functions

Date: 2026-11-08 · Branch `codex/ps2recomp-a18-calls`, based on `codex/ps2recomp-a17-branches`
(integration line: Crash E3 runtime + A11–A17). Read `docs/BRANCH-MAP-2026-11-08.md` first.
Pinned upstream `e5636981…`. No runtime source changed (`tools/check_crash_files_unchanged.sh` OK).

## What is accepted

Real-generator fixture `tests/fixtures/ps2recomp_a18/` (8 functions; the two negative ones
produce 2 upstream warnings, 0 errors):

- `jal target` with a real or elided-NOP delay slot, from a function that otherwise uses the
  already accepted subset and ends in `jr $ra` (callers spill `$ra` with `sw/lw`).
- Exact upstream text per call: `ctx->pc = J;`, `SET_GPR_U32(ctx, 31, RET);`, optional slot
  block, `ctx->pc = TARGET;`, `if (!runtime->dispatchGuestBranch(rdram, ctx, TARGET, J, RET,
  PS2Runtime::GuestBranchKind::DirectCall, "JAL")) { return; }`, `ctx->pc = RET;` — all hex in
  uppercase (confirmed with addresses containing letters). `RET = J + 8` gets a
  `label_RET:` and a `case RET: goto label_RET;` entry in the resume switch (now: backward-branch
  targets ∪ call return addresses, excluding the function start).
- The callee is only a target address. It may be another lowered function or code left to
  the interpreter; the parser does not need to see it.

Rejected, with real upstream output kept as negatives: `j` to another function (emitted as a C++
call `a18_callee_0x100000(rdram, ctx, runtime); return;`) and `jalr` (indirect call via
`GuestBranchKind::IndirectCall`, heavy label/`fallthrough` scaffolding), plus every tamper case.

## Lowering

The call and its slot end an AOT block; the return address starts the next block. At run time
the existing flow does the work with no scheduler change: `jal` links `$ra` and sets the next PC,
the callee runs natively (own AOT entries) or on the interpreter, its `jr $ra` lands on the
return-block entry. `report.private.json` now counts blocks, branches and calls.

## Evidence (host and modelled PPC only)

- Parser: **78 tests OK** (new: golden calls/blocks, hex-case checks, real negatives, every-line
  deletion/insertion and exhaustive number mutation of all callers, call-semantics tampering,
  ELF binding of all slots, lowering equals `generate(blocks)`).
- Host differential, source-only and ELF-bound: **720** comparisons per mode with all six
  functions lowered (callee runs natively; 504 partial-budget, 72 fault/device, 18 live-source), and a
  second configuration where **only the callers are lowered** so each callee retires on the
  interpreter (360 comparisons per mode). The whole synthetic ELF image is guest memory in all
  runs; sp points into valid RAM. Altering the generated link value aborts the run.
- Linked PPC (Unicorn, devkitPPC r51 ELF): **270** (all lowered) and **159** (callers only)
  comparisons; A11–A17 results unchanged.
- Gates: see the same table as A17 in `docs/PS2RECOMP-A17-COND-BRANCHES.md` (92 Crash host gates,
  1024 VU-AOT, bridge/hybrid tests, Wii build, linked VU/VIF checks).

## Limits

No `jalr`/`j`/recursion-specific handling, no calls whose callee returns elsewhere (a changed
`dispatchGuestBranch` result is not modelled), ≤ 32 words per function and block. No Dolphin or
Wii run; no performance claim.
