# PS2Recomp scheduler two-phase API — Batch A6 audit

Date: 2026-11-08
Branch: `codex/ps2recomp-bridge-batch-a-memory`
PR: #58

## Outcome

**A6 implementation is blocked by a more fundamental integration gap.** The existing native-window gate is not part of the live EE scheduler execution loop. Adding a prepare/commit API around it now would create a second execution/accounting path rather than a safe scheduler-owned lease.

No runtime or guest behavior was changed in this batch.

## Verified call graph

The source-level call-site audit of `source/core/ee/ee_core.c` found:
- `ee_core_native_window_begin_with_addresses()` and
  `ee_core_native_window_begin_aot()` are wrappers around the native-window
  admission implementation. Their only occurrences in this translation unit
  are the definitions/wrapper calls.
- The bridge calls the native-window API, and
  `tests/test_ps2recomp_bridge_window.c` exercises it directly.
- The live `ee_core_step_n()` loop executes `ee_step_budget()`, accounts
  completed/parked/fault slots, and calls `ee_core_interleave_account()`.
  It does not invoke the native-window gate.
- `ee_core_interleave_account()` owns the eight-slot IOP boundary and event
  callback progression. Native-window admission currently does not call it.

This means the existing native-window API can advance architectural EE state
in an isolated test/bridge call, but it is not currently a native execution
route integrated into the actual EE/IOP scheduler loop.

## Why a two-phase wrapper alone is insufficient

A prepare/commit wrapper around the current gate would still leave unresolved:
1. How a prepared block is selected by `ee_step_budget()` and how it returns
   its exact scheduler-slot consumption to `ee_core_step_n()`.
2. How its completed instruction count contributes to the returned
   `retired` count while parked/fault slots remain distinct from retirements.
3. How `ee_core_interleave_account()` receives the consumed slots so IOP
   catch-up and event deadlines cannot be skipped or double-counted.
4. How the existing precise JIT/interpreter fallback owns source validation,
   exceptions, branch/delay-slot handling, and canonical write-through state.
5. How native execution remains bounded by the current eight-slot IOP quantum
   and event-width horizon.

If the bridge commits counters itself and the scheduler also accounts slots,
the quantum is double-consumed. If the bridge does not account them, IOP/event
service can be starved. Either outcome is unacceptable for Gekko2's primary
Wii target.

## Required A6 implementation order

1. **Integrate a native-block result into the existing scheduler path.** Add
   an internal result contract for `ee_step_budget()` (or its immediate
   caller) that distinguishes no native entry, native instructions retired,
   scheduler slots consumed, and a precise fallback/fault result.
2. **Keep scheduler ownership in `ee_core_step_n()`.** The scheduler, not
   the bridge, must call `ee_core_interleave_account()` exactly once for
   native-consumed slots. The existing IOP callback/event epilogue remains the
   single owner.
3. **Only then add prepare/execute/commit.** The prepare phase must be
   side-effect-free with respect to guest architectural state and scheduler
   clocks. Execute only a fixed integer leaf with no memory, branches,
   delay slots, exceptions, COP/FPU/VU, MMIO, or helper calls. Commit only the
   exact full fixed-length result initially.
4. **Add partial completion only with generated-code polling.** A later
   contract may report a precise retired prefix and resume PC, but only if
   generated code can stop at architecturally valid boundaries before an
   event/deadline. Never infer partial progress from a host return address.
5. **Differentially test the full scheduler.** Compare scalar and native paths
   for architectural state, retired count, consumed slots, Count/Compare,
   timers, display/VBLANK, IOP ticks, interrupt delivery and resume PC.

## Tests required before bridge exposure

- The native route is demonstrably reached from `ee_core_step_n()`, not only
  from a direct unit-test call.
- One native block consumes exactly its instruction slots and triggers the
  same IOP/event boundary as scalar execution.
- Blocks are clamped to the remaining eight-slot quantum and current event
  horizon.
- Native refusal leaves all guest state and scheduler state unchanged before
  precise fallback.
- Fault/unsupported instruction returns to the precise path without duplicate
  retirement or slot accounting.
- Full scalar/native differential tests cover timer, VBLANK, IRQ, DMA, GIF,
  VU, HLE park, halt and branch/delay-slot boundaries.

## Evidence boundary

This is a source call-graph audit of the PR branch, not a runtime benchmark.
No external PS2Recomp code executes; no Wii/Dolphin/physical-Wii build or
behavior is claimed. The previous A4/A5 host CI remains evidence for the
existing bridge/synthetic tests only, not for scheduler-integrated native
execution.


## Batch A8 correction — generated AOT is already scheduler-integrated

A follow-up audit of `tools/ee_aot_generate.py` found an important path
missed by A6: the generated AOT executor is selected inside the live
`ee_step_budget()` path. In a `GEKKO2_EE_AOT` build,
`ee_aot_try_execute_fetched()` selects the entry and calls its generated
function. For a pure integer prefix of at least two instructions, that
generated function calls `ee_core_native_window_begin_aot()`; after a
successful grant it executes the generated body and calls
`ee_core_interleave_account(fast)`. Its scalar AOT fallback accounts each
retired instruction with `ee_core_interleave_account(1u)`.

`ee_core_step_n()` adds the returned `block_retired` count without a second
interleave call; its own one-slot accounting is used only when no block
retired. Thus the existing generated-AOT path has a scheduler-owned call
site and exactly-once interleave accounting. A6's statement that the
native-window gate is absent from the live scheduler path was too broad:
the *bridge wrapper* was out-of-band, but the generated AOT path is not.

This does **not** mean external PS2Recomp-generated functions are integrated.
The checked-in generated include is empty by default; AOT source is generated
locally from an explicit profile/ELF or by the isolated hybrid compiler.
That compiler uses Gekko2's own AOT translator, not upstream PS2Recomp's
generated C++ runtime. The bridge entry remains fail-closed after A7.

A8 adds a source/generator regression that pins this call path and accounting
contract. The next real integration is to lower a validated PS2Recomp integer
leaf into the existing Gekko2 AOT entry ABI (or an equivalent scheduler-owned
entry), with state, register-clobber, source identity and exact retirement
proofs. Do not add a parallel bridge-side executor or a second interleave
epilogue.
