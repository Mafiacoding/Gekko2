# PS2Recomp EE state adapter — Batch A2 audit

Status: **audit complete; writable architectural-state adapter intentionally deferred**.

This document records the current Gekko2 state contract before introducing any external
PS2Recomp-facing register import/export API. It is not a claim that an external
PS2Recomp runtime or Crash function has executed.

## Canonical state and layout facts

Source of truth: `include/core/ee/ee_core.h` (`ee_state_t`) and
`source/core/ee/ee_core.c`.

- `gpr[32]` are 128-bit values (`ee_reg128_t { ud0, ud1 }`). GPR0 is
  architecturally hardwired and is re-zeroed by the execution path.
- `hi` and `lo` are also 128-bit and deliberately adjacent to `gpr[32]`.
  The PPC dynarec depends on this exact layout; `ee_jit.c` contains static
  offset assertions and passes `&st->gpr[0]` to generated code.
- Architectural control includes `pc`, `next_pc`, `sa_reg`, `cop0[32]`,
  `branch_pending`, the 48-entry TLB, COP1 state, COP2/VU0 state, and
  exception-visible COP0 fields.
- The state struct also contains substantial subsystem state: VU0 micro and
  macro state, VU timing/pipeline, VU local memories, RAM/scratchpad pointers,
  BIOS pointer, halt/idle state, counters, and runtime metadata. It is not a
  register-file-only object.

## Execution-boundary constraints

- `branch_pending` is consumed and cleared at the start of the next
  `ee_step()`; that instruction's exception context determines Cause.BD/EPC.
  A register adapter must preserve the exact pair `pc`/`next_pc` and the
  pending-delay-slot bit. It must not normalize a pending branch into
  `next_pc = pc + 4`.
- `exc_this_pc`, `exc_in_delay_slot`, `exc_raised_this_step`, and
  `mem_tlb_miss` are transient per-instruction exception scratch. They are
  not a portable architectural context. The existing bridge already rejects
  memory callbacks when the last two fault flags are dirty.
- The native PPC code has a live `gpr_generation` dependency. The field is
  documented in `ee_core.h` as invalidating native resident copies after an
  external GPR mutation. `ee_jit.c` passes its offset into the resident and
  fused native emitters. Any future imported GPR/HI/LO values must invalidate
  resident state according to the actual generation contract.
- The native-window gate is a separate scheduler admission contract. It is
  not permission to mutate architectural state at an arbitrary time or to
  yield while a native body is in flight.

## Reset and checkpoint facts

- `ee_core_init()` cold-resets the canonical singleton with
  `memset(&g_state, 0, sizeof(g_state))`, then initializes hardwired VU0/VU1
  state and devices. A bridge context must not outlive/reuse a stale state
  pointer across reinitialization.
- `checkpoint_save()` writes the complete `ee_state_t` as the `EES1` block
  and EE RAM separately. `checkpoint_load()` restores the struct with a raw
  `memcpy`, rebinds RAM/BIOS and DMA pointers, then explicitly invalidates
  process-local JIT source/mapping stamps. This is an implementation-specific
  same-build checkpoint format, not a stable external ABI.
- The checkpoint format's raw struct persistence means a new bridge ABI must
  not be confused with the existing checkpoint layout or use `sizeof(ee_state_t)`
  as a public wire-format version.

## Safe next API shape

A follow-up implementation should use a separate, explicitly versioned POD
context with fixed-width fields, not expose `ee_state_t` or serialize its
bytes. It should be split into:
1. A read-only architectural snapshot for diagnostics and initial integration.
2. A narrow write/commit operation for a proven supported execution profile.
3. Explicit rejection when the context is in a delay slot, exception delivery,
   VU activity, interrupt/device boundary, halted/idle state, or any other
   unsupported condition.
4. A single audited invalidation path for GPR/HI/LO imports, with generation
   semantics tested against native-resident code.
5. Separate tests for 128-bit GPRs, HI/LO, SA, PC/next-PC, branch-pending
   preservation, GPR0, exception rejection, and generation invalidation.

## Blocking item before writable adapter

The audit confirms that generated resident code consumes the
`gpr_generation` offset, but it does not yet establish a complete, centralized
set of all generation writers and every code path that may hold resident GPR
values. Until that mutation/invalidation contract is traced end-to-end, a
writable adapter could make stale native register copies observable. Therefore
this batch deliberately adds no speculative register mutation API and no
unsafe direct struct overlay.

## Validation boundary

This is a source audit only. It has not changed CPU execution, scheduler
behavior, graphics, DMA, or Wii code generation. No Wii build, Dolphin run,
physical-Wii test, FPS result, Crash ELF execution, or external PS2Recomp
runtime result is claimed.


## Batch A3 — resident GPR invalidation trace

The source trace now covers every C/C++ source/header under `source/core/` on
this branch (22 files), plus the EE block policy and residency implementation.

- The EE `gpr_generation` writer was the real EE thread `load_context()`
  path in `source/core/ee/ee_hle_thread.c`. It increments immediately before
  replacing the live 128-bit GPR file and associated PC/HI/LO/SA/FPU context.
- `source/core/iop/iop_core.c` has similarly named increments, but those
  belong to the separate `iop_state_t` and must not be conflated with EE.
- EE block prepare/boundary callbacks do not directly write guest GPRs.
  Their retirement path may reschedule; an actual EE thread replacement
  reaches `load_context()` and bumps the EE generation.
- EE HLE syscall return writes (for example V0) are in the scalar syscall
  path. Syscall encodings are not admitted by `ee_jit_block_candidate()`,
  so they do not run inside a resident native block.
- `ppc_residency_internal.h` reserves PPC r18-r29 for selected EE GPR words
  and r30 for the generation snapshot. `residency_refresh()` compares the
  live generation and reloads all valid cached words when it changes.
  `residency_body()` retains canonical write-through stores and rejects
  unknown/helper/control bodies as residency fences. Resident host registers
  are saved/restored around each callable block; they are not carried across
  independent JIT calls.
- The existing `tests/test_ee_residency_generation.c` invokes the real
  `load_context()`, verifies both halves of a 128-bit GPR, and checks natural
  uint32 wraparound.

### A3 code change

Added `ee_core_gpr_generation_bump(ee_state_t *)` as the single explicit
invalidation hook for out-of-line EE GPR writers. The real thread
`load_context()` now uses this hook. The residency regression additionally
checks explicit bump, null safety, and wraparound. Any future PS2Recomp
snapshot commit that writes EE GPRs must call this hook after committing the
register values; it must not treat the counter as a global state epoch.

This closes the identified invalidation integration point, but it does not
make a writable state adapter safe by itself: A4 must still define an
architectural profile, commit only at an admitted scheduler boundary, and
reject all state it cannot represent. No external PS2Recomp execution is
claimed.
