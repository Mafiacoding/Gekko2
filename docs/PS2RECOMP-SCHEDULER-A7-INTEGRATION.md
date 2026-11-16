# PS2Recomp scheduler integration — Batch A7

Date: 2026-11-08
Branch: `codex/ps2recomp-bridge-batch-a-memory`
PR: #58

## Outcome

**Safety hardening implemented; native execution integration is not yet implemented.**

A6 established that the bridge's native-window gate is outside the live
scheduler path. A7 closes that unsafe out-of-band path rather than adding a
second scheduler or pretending that a wrapper is a lease.

## Source-grounded path

- `ee_core_step_n()` calls `ee_step_budget(n-used, &block_retired)`.
- It adds actual retired instructions to `retired` and `used`.
- When no instruction block retired, it still consumes one scheduler slot
  and calls `ee_core_interleave_account(1)` so parked/fault/redirect attempts
  cannot starve IOP progress.
- `ee_core_interleave_account()` owns the interleave quantum and invokes
  the IOP/event callback at the correct boundary.
- The bridge previously called
  `ee_core_native_window_begin_with_addresses()` directly. That admission
  routine publishes the full grant's instruction count, COP0 Count,
  display/timer clocks and PC/next-PC before returning. It does not report
  scheduler slots to `ee_core_step_n()` and does not call the scheduler's
  interleave epilogue.

## A7 code changes

1. `ee_recomp_bridge_begin_native_window()` now validates the request/handle
   but always returns zero. It performs no guest or scheduler mutation.
2. The header documents this as a reserved, fail-closed compatibility entry.
3. The bridge-window regression now proves a valid-looking bridge request
   leaves the entire `ee_state_t` and display clock unchanged.
4. Existing internal native-window admission tests remain direct tests of
   the Gekko2 gate's source/device/event/budget/memory guards. They are no
   longer mistaken for proof of bridge-to-live-scheduler integration.
5. The Claude handoff records the current boundary and the next integration
   contract.

## What A7 does not claim

- No native PS2Recomp code executes.
- No native block is selected by `ee_step_budget()`.
- No scheduler slots are retired by the bridge.
- No new partial-retirement, cancellation, or resume-PC protocol exists.
- No guest instruction semantics, graphics, GX, IOP, timer or scheduler
  behavior was intentionally changed.
- No Wii build, Dolphin execution, physical-Wii run or FPS result is claimed.

## Next implementation contract

The eventual executor must be selected inside `ee_step_budget()` and return
a typed result distinguishing no-entry/fallback, exact retired instructions,
scheduler slots consumed, and precise fault/redirect/park outcomes. The
scheduler must own the one and only call to `ee_core_interleave_account()`.
The first native leaf should be fixed-length and straight-line integer-only,
with no memory, branches/delay slots, exceptions, helpers, COP/FPU/VU, MMIO
or asynchronous exits. Event horizons and the remaining eight-slot quantum
must clamp the grant. Partial completion is out of scope until generated code
can poll at architecturally valid instruction boundaries.

## Validation

GitHub Actions validation is pending. This document is an implementation
handoff, not test evidence.


## Batch A8 — corrected live AOT call-graph audit

A6 audited the bridge call graph but did not follow the generated AOT source
emitted by `tools/ee_aot_generate.py`. That generated code is invoked from
`ee_step_budget()` through `ee_aot_try_execute_fetched()`; successful
pure-window grants call `ee_core_interleave_account(fast)` inside the
generated entry, while scalar AOT steps call it once per retired instruction.
`ee_core_step_n()` does not account a second time when `block_retired` is
nonzero. The existing AOT route is therefore scheduler-integrated.

A8 corrects the audit and adds a regression for this source/generator
contract. This does not open the fail-closed bridge entry and does not execute
external PS2Recomp output. The default checked-in generated AOT include is
empty; private/build-time generated blocks are required for that route.
