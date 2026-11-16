# PS2Recomp scheduler lease audit — Batch A5

Date: 2026-11-08
Branch: `codex/ps2recomp-bridge-batch-a-memory`
PR: #58

## Result

**A5 is audit-complete; the execution-lease implementation is blocked on a scheduler API redesign.** No speculative lease/token API was added, and no guest execution, scheduler accounting, graphics, or Wii behavior was changed.

The current bridge entry point is not an execution lease. It is a call-through to the existing native-window admission gate, whose successful return means that the entire returned window has already been accounted architecturally.

## Source trace

The bridge function `ee_recomp_bridge_begin_native_window()` in
`source/core/recompiler/ps2recomp_bridge.c` checks the handle, clean transient
fault state, expected PC/next-PC, and basic halted/idle/branch conditions. It
then calls `ee_core_native_window_begin_with_addresses()`.

The implementation in `source/core/ee/ee_core.c`,
`ee_core_native_window_begin_impl()`, performs conservative admission:
- native-event and EE-service options must be enabled;
- the state must be the live singleton `&g_state`;
- VU0/VU1, XGKICK, GIF FIFO, DMA, RPC/SIF, INTC/DMAC, timer, display/VBLANK,
  COP0 Compare/interrupt and interleave constraints must permit the window;
- source words must be proven live for the non-trusted path;
- special wait/unblock PCs and unsafe memory dependencies are rejected;
- count is bounded by the requested budget, interleave limit and maximum
  native block size.

These checks are valuable, but after they pass the function immediately
publishes the whole grant:
- `instructions_executed += n` and COP0 Count is advanced;
- Cause pending bits and timer/display clocks are updated;
- exception scratch is reset for the last instruction;
- `pc` and `next_pc` are advanced to the end of the entire window.

Only then does it return `n`. The existing bridge comment therefore correctly
requires the caller to execute exactly the admitted body immediately and not
retire it again. There is no active lease object, token, cancellation path,
partial-retirement report, end/commit call, typed exit reason, or resume-PC
calculation.

The existing per-instruction block boundary APIs are not a substitute for a
cross-runtime lease: they retire a completed instruction and then prepare the
next one through Gekko2's existing JIT path. Their semantics are coupled to
canonical write-through state and the existing generated-body calling
convention.

The scheduler's event/interleave callback machinery is owned by
`ee_core_step_event_n()` / `ee_core_step_n()`. A bridge caller cannot safely
assume it can invoke or skip that epilogue independently. The native-window
gate's conservative checks are not a persistent reservation against all
future asynchronous device/event changes.

## Why a wrapper-only lease would be unsafe

If external generated code exits after fewer than `n` instructions, the live
EE state and scheduler clocks already claim that all `n` instructions ran.
If it faults, encounters an unsupported operation, or needs an event service,
the bridge has no way to undo or accurately reconcile those effects. Returning
a guessed PC or subtracting a guessed instruction count would corrupt
architectural state and timing. A token that merely records the pre-advanced
state would not fix this.

## Required scheduler redesign before external entry

1. **Separate admission from publication.** A prepare operation must return a
   call-scoped grant without advancing PC, Count, timers, display clocks or
   retirement counters.
2. **Bind the grant to its source.** Include start PC, exact instruction count,
   expected sequential PC, source words/hash or validated source identity,
   state identity, and relevant state/code generation. Revalidate before
   entry where mutation can occur.
3. **Define a strict execution profile.** Initially permit only a fixed,
   straight-line integer leaf: no branches/delay slots, memory, exceptions,
   COP/FPU/VU, MMIO, helper callbacks, or writes outside the declared
   architectural register set. Unsupported code must fall back before entry.
4. **Define completion accounting.** A successful fixed-length return must
   report the exact number of instructions executed. Any future early-exit
   support must return the exact retired prefix and precise resume PC, and
   generated code must poll/exit at architecturally valid boundaries.
5. **Centralize commit and scheduler epilogue.** Gekko2 alone publishes PC,
   Count, timer/display progress, interrupt state and retirement totals for
   the actually completed prefix. Do not run a second retirement path.
6. **Make ownership synchronous.** Keep one owner of the EE state during the
   call. No persistent global lease unless scheduler serialization and all
   mutation sources are proven. Event/device servicing must occur through the
   normal scheduler owner after the native call returns.
7. **Use typed exits.** At minimum: completed, budget exhausted, event/deadline,
   interpreter required, source/state stale, guest fault, and invalid grant.
   Every exit must define the exact resume PC and whether any instruction was
   retired.

## Tests required for the redesigned contract

- Admission refusal leaves the entire EE state and all scheduler clocks
  unchanged.
- Grant creation alone leaves PC, Count, instruction totals, timer/display
  clocks and pending interrupt state unchanged.
- Full fixed-length completion commits exactly the grant once.
- A reported partial prefix commits only that prefix and resumes at
  `start_pc + 4 * retired_prefix` for the restricted sequential profile.
- Zero-retirement fault/refusal commits nothing and resumes at start PC.
- Stale source/state, budget zero/one, special PCs, VU/DMA/GIF, pending events,
  Count overflow, display/VBLANK boundary and timer deadline all fail closed.
- Differential tests compare the complete supported architectural state
  against the interpreter; event/device epilogue tests prove no scheduler
  callback is skipped or double-run.

## Next step

Implement the scheduler-side two-phase grant/commit API and its tests first.
Only after those pass should the bridge expose a typed lease and permit a
synthetic PS2Recomp leaf to execute. The existing
`ee_recomp_bridge_begin_native_window()` remains a legacy all-or-nothing
admission/accounting call and must not be reinterpreted as a lease.

## Evidence boundary

This is a source-level audit of the current branch. It is not evidence of
external PS2Recomp PPC execution, Wii compilation, Dolphin execution, physical
Wii behavior, FPS improvement, or Crash compatibility.
