# PS2Recomp ↔ Gekko2: Shared Runtime Bridge Design

Status: **design and acceptance contract only**. This document does not claim that
PS2Recomp is integrated into the public Gekko2 runtime.

## 1. Why this document exists

Gekko2 already has a substantial native EE execution path. The public tree
contains the canonical `ee_state_t` in `include/core/ee/ee_core.h`, the
generated EE AOT interface in `include/core/recompiler/ee_aot.h`, and the
existing interpreter/JIT, memory, exception and device paths. The EE state
already represents 128-bit GPRs and HI/LO, SA, PC/next-PC, branch-delay state,
COP0 and TLB state.

Therefore, **do not introduce a second miniature 32-bit EE register context**.
The integration problem to solve is the boundary between a separately built
PS2Recomp runtime and Gekko2's existing EE, memory, scheduler, device and
graphics ownership.

This is a design gate, not an implementation report. No native PS2Recomp
function is admitted by this document, and no Wii/Dolphin/physical-hardware
result is implied.

## 2. Goals and non-goals

Goals:
- Define one authoritative owner for guest CPU state and device state.
- Reuse Gekko2's canonical EE state and existing memory/exception helpers.
- Make native-function entry and exit explicit, bounded and testable.
- Preserve interpreter fallback for unsupported or dynamically resolved code.
- Keep DMA, VU/VIF, GIF, GS and GX side effects serialized through Gekko2.

Non-goals for the first milestone:
- A standalone PS2 game loader or ISO/CHD-to-DOL converter.
- A new EE interpreter or a replacement for Gekko2's existing interpreter.
- A new graphics renderer, GS implementation or GX backend.
- VU/GS native-function admission.
- A claim of Crash gameplay, BIOS speedup, graphics correctness or 30 FPS.

## 3. Ownership model

At any instant, exactly one engine owns active guest EE execution.

- **Gekko2 interpreter/JIT/AOT:** Gekko2 owns `ee_state_t`, guest memory
  access, exception delivery, scheduler deadlines, DMA/device state and GX
  presentation.
- **PS2Recomp native function:** a narrowly admitted function may execute only
  after Gekko2 yields a complete, validated execution lease. The lease records
  guest PC, source identity, state generation, permitted operations and a
  finite instruction/time budget.
- **Return to Gekko2:** the native function returns an explicit exit reason and
  a complete architectural update. Gekko2 resumes event/interrupt processing
  before granting another native lease.

Two engines must never concurrently mutate the same EE state, guest RAM write
stream, DMA channel, VU pipeline, GS register set or VRAM. A native function
must not independently advance a device that Gekko2 also services.

## 4. Architectural state contract

Gekko2's existing `ee_state_t` is the canonical Gekko2-side state. Do not
publish its raw C structure layout as a stable PS2Recomp ABI: it contains
emulator-specific and transient fields, and its layout is not a versioned
cross-runtime serialization format.

An adapter must explicitly account for every architectural field observable
or writable by the admitted function:

- all 32 EE GPRs, including both 64-bit halves of each 128-bit register;
- immutable GPR-zero semantics;
- full HI/LO, SA, PC, next-PC, branch-pending and delay-slot state;
- COP0, TLB and exception state required by the admitted opcode set;
- FPU and VU state whenever reachable code can access it;
- retirement accounting and the scheduler/event budget for the lease.

The existing `ee_reg128_t` convention stores the low 64 bits in `ud0` and
the high 64 bits in `ud1`. The adapter must document and test lane ordering;
it must not infer it from host endianness. Partial import/export is forbidden
unless static analysis and runtime guards prove omitted fields cannot be
observed or modified.

Each lease carries a state generation. Reject stale leases after reset,
exception delivery, checkpoint restore, code invalidation or any other
mutation that invalidates the lease assumptions.

## 5. Memory and MMIO contract

Generated code must never treat an arbitrary guest address as a host pointer.

The first implementation uses Gekko2 memory callbacks for all guest accesses.
They must preserve guest byte order, alignment behavior, address translation,
TLB faults, exception delivery and device side effects.

A future direct-RAM window may be considered only if it carries:
- physical range and read/write permissions;
- mapping and source-generation identifiers;
- an invalidation token;
- a proof that the entire access is ordinary RAM, not MMIO or a device window.

Decline direct access for MMIO, translated addresses without a validated
mapping, cross-boundary accesses or any range with unrepresented side effects.
Writes must notify the existing JIT/AOT code invalidation mechanism. A native
path must not silently turn an MMIO access into a byte-array operation.

## 6. Scheduler, interrupts and exit reasons

Every native entry receives a finite budget no larger than the current Gekko2
scheduler grant. Native execution must yield before the earliest of:
- retirement budget exhaustion;
- an interrupt/event deadline or scheduler quantum boundary;
- exception, syscall, trap or unsupported instruction;
- unresolved indirect control flow;
- memory/MMIO operation the bridge cannot service precisely;
- state-generation or source-identity guard failure.

The bridge must distinguish at least these exit reasons:

- `COMPLETED`: declared function return/exit reached;
- `BUDGET_EXHAUSTED`: yielded at a precise resumable guest PC;
- `EVENT_PENDING`: yielded before Gekko2 services an event/interrupt;
- `INTERPRETER_REQUIRED`: Gekko2 resumes at the returned guest PC;
- `STATE_INVALID`: lease invalidated; discard native assumptions;
- `FAULT`: Gekko2 must deliver the architectural exception.

No native function may cross a scheduler boundary merely because it is one
host function. Do not fabricate instruction retirements. If return PC, delay
slot or exception state is ambiguous, decline native admission and use
Gekko2's interpreter.

Do not claim a general PS2Recomp interpreter fallback unless a real, tested
interpreter exists for the relevant instruction and state. A missing-PC
diagnostic is not an interpreter.

## 7. Device and graphics ownership

The first bridge milestone does not transfer device ownership. Gekko2 remains
the sole owner of:
- EE/IOP scheduling and event deadlines;
- DMA channels and completion signals;
- VIF/VU execution and pipeline timing;
- GIF/PATH backpressure and transfer ordering;
- GS registers, local memory/VRAM, scanout and GX presentation;
- interrupt delivery and checkpoint/reset lifecycle.

A PS2Recomp function that needs one of these services yields a typed request.
Gekko2 performs the operation and returns a completion result. Unknown requests
fail closed; they must not be treated as successful no-ops.

A future shared GS/GX bridge requires a separate ownership design and
image-level differential tests. It is outside the first EE bridge milestone.

## 8. Native-function admission

Admit a function only when all conditions hold:
- guest address range and source bytes are known and hashed;
- entry ABI, return convention, clobber set and delay-slot behavior are explicit;
- reachable instructions and indirect targets are covered or guarded;
- memory, MMIO, exception and device effects are modeled or cause a precise yield;
- writes, overlays, relocations and source-generation changes invalidate the entry;
- differential tests compare it with Gekko2's interpreter from identical state.

Function names, ELF symbols or a successful scalar leaf probe alone are not
sufficient evidence for admission.

## 9. Required test gates

### Gate A — state adapter (host)
- Round-trip all 128 bits of all GPRs, HI/LO, SA, PC/next-PC and branch state.
- Test GPR zero, upper lanes, signed/unsigned boundaries and lane ordering.
- Prove unsupported state fields cause admission rejection.
- Reject stale generations after reset, exception and checkpoint restore.

### Gate B — memory and faults (host)
- Compare byte/half/word/double/quadword accesses with Gekko2's helpers.
- Cover endianness, alignment, aliases, TLB faults and RAM boundaries.
- Prove MMIO uses typed callbacks, not direct RAM.
- Prove writes invalidate affected translated/native code.

### Gate C — scheduler and control flow (host)
- Budgets 1, 2, 4 and boundary-adjacent values.
- Taken/not-taken branches, delay-slot entry/exit and branch targets.
- Events due immediately before, during and after a native region.
- Syscall, trap, unsupported opcode and indirect-target fallback.
- Native → interpreter → native with architectural state snapshots.

### Gate D — device requests (host)
- DMA/VIF/VU/GIF/GS requests preserve ordering and completion semantics.
- Duplicate ownership is rejected.
- Unknown requests fail closed.
- Device state is unchanged when no request was issued.

### Gate E — target evidence
Host tests are not Wii evidence. After all host gates pass, build a fresh Wii
DOL/ELF and record hashes. Test Dolphin separately and then a physical Wii.
Report these platforms independently. No FPS, BIOS, graphics, stability or
Crash-playability claim is permitted without a matching run and measurements.

## 10. Recommended implementation sequence

1. Add a small versioned bridge interface with opaque runtime handles and typed
   exit/request enums. Do not expose `ee_state_t` layout to generated code.
2. Implement state import/export against canonical `ee_state_t`; reject fields
   the adapter cannot preserve.
3. Implement callback-only memory operations and fail-closed device requests.
4. Add Gate A–D host tests before admitting any Crash function.
5. Admit one pure integer leaf function, then one memory-using function, each
   with a differential oracle. Keep VU/GS/GX functions out of scope.
6. Only after the host contract is stable, add a Wii build and platform test.

## 11. Evidence boundary

This document defines future acceptance criteria. It does not mean the two
runtimes are integrated. It does not prove Crash E3 works on Wii or Dolphin,
that the BIOS is faster, that GX output is correct, or that 30 FPS is
achievable. Existing Gekko2 AOT/JIT and graphics work remains separate
evidence until a shared bridge is implemented and independently tested.
