# PS2Recomp scheduler integration — Batch A8

Date: 2026-11-08
PR: #58

## Corrected call graph

A6 correctly identified that the bridge wrapper itself was out-of-band, but
its broader claim that native-window admission was absent from the live
scheduler path was incomplete. The generated AOT path was not traced.

The actual route in an AOT-enabled build is:

1. `ee_core_step_n()` calls `ee_step_budget(n-used, &block_retired)`.
2. `ee_step_budget()` calls `ee_aot_try_execute_fetched(st,budget,instr)`.
3. `ee_aot_try_execute_fetched()` bounds the budget with
   `ee_core_interleave_limit()`, looks up the current PC and calls the
   generated entry.
4. `tools/ee_aot_generate.py` emits
   `ee_core_native_window_begin_aot()` for an eligible pure integer prefix
   of at least two instructions.
5. On a successful grant, generated code executes that exact prefix and
   calls `ee_core_interleave_account(fast)`. If the fast grant is absent,
   generated AOT code uses per-instruction prepare/commit and calls
   `ee_core_interleave_account(1u)` for each retired instruction.
6. The generated entry returns its retired count. `ee_core_step_n()` adds
   that `block_retired` count and does not call interleave accounting again.
   Its one-slot accounting branch is only for a non-retiring scalar/park/fault
   attempt.

The generated entry therefore owns the same exactly-once interleave contract
used by the live scheduler; adding another bridge-side accounting epilogue
would double-account.

## A8 change

- Added `test_live_aot_scheduler_path_and_window_accounting` to
  `tests/test_ee_aot_inputs.py`.
- The regression checks the live `ee_step_budget()` AOT dispatch, the
  scheduler's non-double-accounting branch, AOT budget clamping, and emitted
  fast-window/per-instruction interleave calls.
- Corrected the A6 audit and Claude handoff to distinguish the out-of-band
  bridge wrapper from the already-integrated generated AOT path.

## Remaining gap

This is not external PS2Recomp runtime integration. The repository's
checked-in `ee_aot_generated.inc` is empty by default. Local AOT generation
and the isolated ELF-to-AOT hybrid compiler use Gekko2's own translator.
They do not lower or execute upstream PS2Recomp-generated C++.

Next step: implement a narrowly scoped, differential-tested lowering of a
validated PS2Recomp integer leaf into the existing Gekko2 AOT entry ABI,
preserving the 128-bit architectural state, exact source identity, register
clobbers, retirement count, budget and event contract. Do not introduce a
parallel scheduler path.

## Validation

GitHub Actions pending. Source/generator regression only; no external
PS2Recomp execution, Wii build, Dolphin run, physical Wii run or FPS result.
