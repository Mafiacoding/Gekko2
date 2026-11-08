# Claude handoff: PS2Recomp ↔ Gekko2

Status: **guarded memory bridge + synthetic ELF/AOT host pilot implemented; external PS2Recomp runtime is not integrated.**

Working branch: `codex/ps2recomp-bridge-batch-a-memory`
Review: [PR #58](https://github.com/Mafiacoding/Gekko2/pull/58)
Base: current `main` (the branch is intentionally not based on the divergent
`codex/wii-recomp-foundation` line).

## Read first

1. `AGENTS.md`
2. `docs/CODEX_HANDOFF.md`
3. `STATUS.md`
4. `docs/PS2RECOMP-GEKKO2-BRIDGE-DESIGN.md`
5. `docs/PS2RECOMP-BRIDGE-BATCH-A-B.md`
6. `experiments/ps2recomp-wii/HYBRID-BATCH.md`
7. The bridge source/header and the focused tests below.

## Verified implementation on this branch

- `include/core/recompiler/ps2recomp_bridge.h` and
  `source/core/recompiler/ps2recomp_bridge.c` define a versioned opaque handle
  bound to the canonical Gekko2 `ee_state_t`.
- 8/16/32/64/128-bit memory callbacks delegate to Gekko2's existing
  `ee_mem_*` helpers. Guest virtual addresses are never cast to host pointers.
- The bridge native-window entry is currently fail-closed and returns zero.
  Batch A7 removed its direct call to
  `ee_core_native_window_begin_with_addresses()`: that gate advances EE
  retirement/PC/clocks, while the bridge is not part of the live scheduler.
  Internal Gekko2 admission tests remain separate; no external bridge caller
  may advance guest state outside scheduler-owned accounting.
- `experiments/ps2recomp-wii/hybrid_compile.py` conservatively discovers
  supported control-flow regions from synthetic or owner-supplied ELF32
  little-endian ET_EXEC input and emits through Gekko2's existing AOT generator.
  Output is required to be outside the checkout and is created privately.
- `tests/test_native_hybrid_execute.c` binds the bridge to the live EE
  singleton, seeds synthetic code/data through bridge callbacks, then checks
  generated AOT plus interpreter fallback against the scalar path.

## Verified host CI

GitHub Actions run `42486351229`, commit
`526f2993142b9e0fb9f0f95f4af1613fef26c005`:

- Bridge regression: PASS (all five memory widths, TLB fault reporting,
  detached-state native-window refusal).
- Live scheduler admission regression: PASS (live-source, device/event,
  bounded-grant and memory-dependency guards).
- CFG/compiler unit tests: 5/5 PASS.
- Synthetic real-core AOT/interpreter test: 512/512 comparisons PASS,
  512 AOT hits; bridge memory callbacks exercised.
- Host-native evidence only. No Wii cross-build, Dolphin execution, physical
  Wii run, FPS, Crash gameplay or external PS2Recomp runtime result.

## Important branch boundaries

- PR #56 (`codex/native-hybrid-cfg`) is a separate PR based on the divergent
  `codex/wii-recomp-foundation` branch. Only its isolated compiler/test files
  were ported. Do not merge or rebase its 106-commit foundation wholesale.
- PR #54 contains Crash E3 VU/CLIP timing work on a separate
  `codex/crash-e3-local-repro` stack. It is not included in this current-main
  branch. Do not claim that PR #58 contains those fixes.
- Do not upload, commit, or publish any private Crash ELF, BIOS, ISO, CHD,
  memory-card image, RAM dump, generated game code, or owner logs.
- Existing GX, Crash, VIF/GIF/DMA and default optimization behavior must remain
  unchanged unless a focused regression test proves a required fix.
- Do not merge PR #58 without explicit owner approval.

## Next required batches (do not skip gates)

### Batch A2 — explicit architectural state adapter
The current opaque pointer is an in-process ownership handle, not an exported
state ABI. Define a versioned, explicitly laid-out state view only after a
field-by-field audit. Cover 128-bit GPRs, immutable GPR zero, HI/LO, SA,
PC/next-PC, branch/delay state, and every additional field required by the
declared opcode profile. Do not silently reuse `gpr_generation` as a global
state epoch; its current documented role is narrower. Trace reset, exception,
TLB/mapping, checkpoint restore and code invalidation before adding stale-lease
semantics. Reject any state profile that cannot be represented completely.

### Batch C2 — typed execution lease and exit contract
The current native-window wrapper is only a call-through to the existing
Gekko2 admission gate. It is not a complete cross-runtime lease. Before
external PS2Recomp entry, define typed exit reasons (completed, budget,
event-pending, interpreter-required, state-invalid, fault), precise resume PC,
source identity, state generation, and exact scheduler grant. Preserve one
active owner of EE state and do not skip Gekko2's device/event epilogue.

### Batch D — synthetic native leaf first
Use a synthetic pure-integer leaf with a pinned source hash and explicit ABI,
clobber, return and delay-slot contract. Differentially compare the complete
architectural state against Gekko2's interpreter. Then test one memory-using
leaf through callbacks. Only after these pass may an owner-private Crash
function be evaluated locally; no private fixture belongs in this repository.

### Target evidence
After host gates pass, build a fresh Wii DOL/ELF with devkitPPC/libogc, record
hashes, then test Dolphin and physical Wii separately. Never infer hardware
performance or compatibility from host CI.

## Required commands

```sh
tests/run_test.sh test_ps2recomp_bridge
tests/run_test.sh test_ps2recomp_bridge_window
python3 tests/test_native_hybrid_compile.py
python3 experiments/ps2recomp-wii/run_hybrid_test.py
python3 tests/test_ee_aot_inputs.py
tests/run_test.sh test_ee_residency_generation
tests/run_test.sh test_ee_exec_lifecycle
```

Report exact stdout/exit status and keep host, synthetic PPC, Dolphin and
physical Wii evidence separate.


Note: the repository's older `tests/test_ee_aot.c` currently includes
`aot_test_fixture.h`, which is absent from the current-main tree. Do not
report that C test as passing. The workflow instead runs the available
`test_ee_aot_inputs.py` plus the synthetic generated-AOT/interpreter runtime
comparison above.


## Batch A2 — EE state adapter audit (2026-11-08)

The field-by-field source audit is recorded in
`docs/PS2RECOMP-EE-STATE-ADAPTER-AUDIT.md`.

Key result: do **not** add a writable state overlay yet. `ee_state_t` mixes
128-bit architectural registers and branch state with transient exception
scratch, TLB, COP1/COP2/VU state, device/runtime state, and process pointers.
The PPC resident path consumes the `gpr_generation` offset, but this audit
has not yet established every writer and every resident-register invalidation
path end-to-end. A speculative external GPR write API could therefore leave
stale native values live.

Confirmed details include cold reset via `memset(&g_state, 0, sizeof(g_state))`,
raw same-build `EES1` checkpoint persistence of `ee_state_t`, separate RAM
persistence/rebinding, and explicit process-local JIT source/mapping
invalidation after checkpoint restore. These are not an external state ABI.

Batch A2 is therefore **audit-complete, writable-adapter-deferred**. The next
engineering gate is to trace all `gpr_generation` mutations and native
resident lifetime/invalidation, then implement a separate fixed-width
versioned snapshot/commit type with fail-closed profile validation. No CPU
execution, graphics, scheduler, or Wii behavior was changed in this batch.


## Batch A3 — resident GPR invalidation trace

- Audited all 22 C/C++ source/header files under `source/core/` on this branch.
- Confirmed the EE generation writer is the real EE thread `load_context()` replacement path; the similarly named IOP increments belong to the separate IOP state and are not EE epochs.
- Confirmed `ppc_residency_internal.h` keeps r18-r29 as selected guest GPR lanes, r30 as the generation snapshot, reloads valid cached words after a generation change, and retains canonical write-through stores/fences.
- Added `ee_core_gpr_generation_bump(ee_state_t *)` as the explicit invalidation hook for any future out-of-line EE GPR writer. `load_context()` now uses it.
- Extended `tests/test_ee_residency_generation.c` for explicit bump, null safety and wraparound, in addition to the real context-switch and 128-bit register test.

A4 can now build a versioned snapshot/commit adapter against this explicit hook, but must still define and validate a conservative architectural profile and scheduler-boundary contract. No writable state overlay or external PS2Recomp runtime execution is claimed.


### Batch A3 validation

GitHub Actions [run 42487584508](https://github.com/Mafiacoding/Gekko2/actions/runs/42487584508) — **SUCCESS** on code commit `54a91b5c34e45e0bef916726ce2aba433faa74bb`. The updated EE residency-generation test passed, including actual 128-bit EE context replacement, explicit generation bump, null safety and uint32 wraparound. The bridge, live scheduler admission, synthetic AOT/interpreter, AOT input and execution-lifecycle checks also passed. Host CI only; no Wii/Dolphin/physical-hardware or external PS2Recomp runtime claim.


## Batch A4 — read-only integer-leaf EE snapshot

Added a separate fixed-layout, versioned 576-byte EE snapshot for the conservative `INTEGER_LEAF` profile. It includes all 32 128-bit GPRs, HI/LO, SA and PC/next-PC, with compile-time layout assertions. Snapshot admission is fail-closed for halted/idle, delay-slot/branch, active EXL/ERL, active VU0, unaligned/nonsequential PC, nonzero GPR0 and dirty exception/TLB state. It does not serialize raw `ee_state_t`, include device/TLB/FPU/VU state, or grant execution. The API is read-only; no commit API or scheduler lease exists yet. Tests cover lane order and unsupported-state rejection.


### Batch A4 validation

GitHub Actions [run 42487839134](https://github.com/Mafiacoding/Gekko2/actions/runs/42487839134) — **SUCCESS** on code commit `61ef28fccf3e728a916c2457bcc780918ecd5b5a`. The bridge test passed with the snapshot ABI size/offset assertions and snapshot round-trip/rejection checks. The rest of the host suite passed as well, including 512/512 synthetic AOT/interpreter comparisons. Host-only evidence.


## Batch A5 — scheduler lease audit (2026-11-08)

The source-level audit is recorded in
`docs/PS2RECOMP-SCHEDULER-LEASE-AUDIT.md`.

**Result: audit complete; external execution lease deferred.** The current
`ee_recomp_bridge_begin_native_window()` is not a lease. Its scheduler gate
performs conservative admission, then advances the full instruction count,
COP0 Count, timer/display clocks and PC/next-PC before returning the grant.
The caller must therefore execute the exact full body immediately and must
not retire it again. There is no cancellation, partial-retirement report,
typed exit, resume-PC calculation or bridge-owned scheduler epilogue.

A wrapper-only token cannot repair an early exit: the state would already claim
that the full window executed. A5 therefore deliberately adds no unsafe API.
The next implementation gate is a scheduler-side two-phase prepare/commit
contract, with source/state identity, fixed-profile validation, exact
completed-prefix accounting and a centralized scheduler epilogue. The initial
external leaf should be straight-line integer-only with no memory, branches,
exceptions, helpers, COP/FPU/VU or MMIO.

No guest execution, graphics, scheduler behavior, or Wii behavior changed in
A5. This is a source audit only; it does not prove external PS2Recomp runtime
execution or Wii/Dolphin/physical-hardware behavior.


## Batch A6 — scheduler integration gate (2026-11-08)

See `docs/PS2RECOMP-SCHEDULER-A6-INTEGRATION-AUDIT.md`.

A6 is **blocked before API implementation** by a call-graph finding: the
native-window gate is not invoked from the live `ee_core_step_n()` /
`ee_step_budget()` execution path. It is reached by the bridge and direct
regression tests only. The live scheduler owns slot accounting through
`ee_core_interleave_account()`, including the eight-slot IOP boundary and
event callback progression; native-window admission currently does not
participate in that accounting.

Do not add a bridge-side two-phase prepare/commit wrapper yet. It would create
a parallel execution/accounting path and could either double-consume scheduler
slots or skip IOP/event service. The correct next implementation is to
integrate a native-block result into the existing `ee_step_budget()` /
`ee_core_step_n()` flow, keep interleave accounting in the scheduler, and
only then add a call-scoped prepare/execute/commit contract for a restricted
integer leaf. Partial exits require generated-code polling at valid
instruction boundaries.

No runtime, graphics, scheduler or Wii behavior changed in A6. No new CI
result is claimed for this documentation-only audit.


## Batch A7 — fail-closed bridge boundary (2026-11-08)

**Implemented safety hardening; scheduler execution integration remains deferred.**

The live path is `ee_core_step_n()` → `ee_step_budget()`; the scheduler
accounts consumed slots and invokes `ee_core_interleave_account()` exactly
once. The bridge's former native-window call instead entered the admission
gate directly. A successful gate call immediately publishes instruction
retirement, COP0 Count, display/timer clocks and PC/next-PC. It does not
return a result to `ee_core_step_n()`, so the IOP/event epilogue cannot be
guaranteed.

A7 therefore makes `ee_recomp_bridge_begin_native_window()` fail-closed
(always zero) until the native executor is selected from the live scheduler.
The internal Gekko2 admission gate remains intact and its direct guard tests
remain, but it is no longer reachable through this out-of-band bridge API.
A regression now verifies a valid-looking bridge request leaves the complete
EE state and display clock unchanged.

**Not implemented:** no native PS2Recomp execution, no new scheduler result
contract, no partial retirement/resume protocol, and no runtime performance
change. The next safe implementation requires an executor callback/result
contract owned by `ee_step_budget()`, with exact retired slots returned to
`ee_core_step_n()`; only that scheduler may call
`ee_core_interleave_account()`. First pilot must be a fixed straight-line
integer leaf with generated-code boundary guarantees.

Validation pending on GitHub Actions. Host-only tests do not prove Wii,
Dolphin or physical-Wii behavior.


## Batch A8 — corrected live AOT scheduler integration audit

Important correction to A6: the live generated AOT path **is** integrated
inside `ee_step_budget()`. It calls `ee_aot_try_execute_fetched()`, which
dispatches a generated entry. The AOT generator emits
`ee_core_native_window_begin_aot()` for eligible pure integer prefixes and
then calls `ee_core_interleave_account(fast)` exactly once. Scalar AOT
fallback accounts each instruction with `ee_core_interleave_account(1u)`.
When `block_retired` is nonzero, `ee_core_step_n()` does not add another
interleave call. See `docs/PS2RECOMP-SCHEDULER-A6-INTEGRATION-AUDIT.md`
for the correction and A8 test.

This route currently serves Gekko2's own generated AOT C, including the
isolated ELF-to-AOT hybrid compiler. It does **not** execute upstream
PS2Recomp-generated C++ or make the opaque bridge executable. The checked-in
AOT generated include is empty by default. The correct next integration is
a validated lowering/adapter into this existing AOT entry path, not a
parallel scheduler callback. The bridge native-window API remains
fail-closed pending that work.


## Batch A9 — upstream generated-code ABI boundary (2026-11-08)

See `docs/PS2RECOMP-A9-ABI-BOUNDARY.md`.

The corrected A8 scheduler test is green in Actions run
[42492533997](https://github.com/Mafiacoding/Gekko2/actions/runs/42492533997)
(all ten steps). A9 documents the next hard integration boundary:
upstream generated leaves use upstream `R5900Context*` plus RDRAM, while
Gekko2 owns a different `ee_state_t`. The existing unchanged-leaf probe uses
a test-only scalar adapter, not either production runtime ABI.

Do not cast/reinterpret these contexts or link the probe adapter into the
emulator. The bridge native-window compatibility API remains fail-closed.
Gekko2's own AOT entries already run under `ee_step_budget()` and own exact
retirement/interleave accounting. External code must be lowered into that
existing entry contract.

**A9 is an ABI audit/documentation batch, not executable integration.** The
next implementation must consume the exact pinned upstream generated output
through a real parser/IR stage and reject unsupported constructs. Begin with a
straight-line scalar integer leaf, then prove complete state and exact
retirement against the interpreter. The current repo does not contain a
production generated-output parser/IR bridge. Do not claim one exists until
its code and differential tests are committed.


## Batch A10 — pinned upstream generated-C++ format audit (2026-11-08)

See `docs/PS2RECOMP-A10-GENERATED-CPP-FORMAT-AUDIT.md`.

The exact upstream generator source at pinned commit
`ran-j/PS2Recomp@e5636981a1c9de368f6b71e819c3dc188dfedbf4` was inspected
directly. `FunctionEmitter` emits
`void name(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime)`,
an optional known logging preamble, a function address-range comment,
per-instruction address/raw-word comments (when enabled), a matching
`ctx->pc` assignment, and the translator's C++ statement. Straight-line
functions end with a fallthrough `ctx->pc = function.end`.

The pinned translators' exact templates are confirmed for ADDIU, ADDU, XOR,
SLL and canonical NOP. Gekko2's existing AOT translator already accepts the
corresponding instruction encodings. This gives a precise first parser
contract: require comments and exact PC progression, decode each raw word,
reconstruct the expected upstream statement, compare it exactly after
whitespace normalization, reject all unknown syntax, then pass only the
validated words to the existing AOT generator.

The public PR tree was also checked recursively: its 49 `.cpp` files are all
PCSX2 reference sources under `docs/reference/pcsx2/pcsx2/`; no upstream
generated game `.cpp` is committed. The existing generated-leaf probe output
is documented as workspace-private.

**A10 is source-format audit only.** No parser/IR/lowering code or generated
fixture was added; no external C++ executes. The emitter source is not a
substitute for a captured output from running the pinned upstream binary.
Claude should implement the strict parser and golden fixture next, following
the A10 audit's rejection rules. Do not broaden the opcode subset before
complete-state differential tests pass.


## Batch A11 — strict generated-C++ parser and AOT lowering (2026-11-08)

See `docs/PS2RECOMP-A11-STRICT-PARSER.md`.

**Implemented and host-tested.** `tools/ps2recomp_strict_parser.py` parses one
straight-line function of real PS2Recomp output (ADDIU, ADDU, XOR, SLL,
canonical NOP only), reconstructs each statement from the raw MIPS word and
rejects the whole function on any difference. Validated words (only) go to
`tools/ee_aot_generate.py --manifest`; the entry runs through the existing
`ee_step_budget()` / `ee_aot_try_execute_fetched()` path with unchanged
interleave accounting. Upstream C++ is never executed; no `R5900Context`
cast; bridge native-window API remains fail-closed.

A **real golden fixture** was produced by building the pinned upstream
generator (`e5636981…`) and running it on a synthetic ELF
(`tests/fixtures/ps2recomp_a11/`, with `PROVENANCE.json`). Running the real
binary corrected A10: names are `<name>_0x<start>`, negative ADDIU immediates
print as unsigned 32-bit decimal, and an extra entry `ctx->pc` line exists.

Tests: `tests/test_ps2recomp_strict_parser.py` (24, positive/negative),
`tests/test_ps2recomp_a11_aot_diff.py` (240 interpreter/AOT comparisons incl.
128-bit GPR upper lanes, GPR0, retirement, live-source guards). Both are in
the host CI workflow. No Wii build (no devkitPPC in the container), no
Dolphin/hardware result, no performance claim.

Next (A12): widen one instruction class at a time with a real-generator
fixture each; then `jr $ra` + delay slot; do not skip the differential gate.


## Batch A12 — pure integer ALU class (2026-11-08)

See `docs/PS2RECOMP-A12-INTEGER-ALU.md`. Parser extended (table driven) with
SLTI, SLTIU, ANDI, ORI, XORI, LUI, SUBU, AND, OR, NOR, SLT, SLTU, each backed
by a real-generator fixture (`tests/fixtures/ps2recomp_a12/`) and differential
(360 + 9 guard cases). Host-only evidence.


## Batch A13 — `jr $ra` terminal with delay slot (2026-11-08)

See `docs/PS2RECOMP-A13-JR-RA.md`. Real-generator fixture
`tests/fixtures/ps2recomp_a13/`. Parser accepts exactly the upstream `jr $ra`
block (uppercase hex in branch blocks, elided NOP slots lowered as an inferred
word 0 guarded by the live-source check, jump target read before the slot).
Tests: 39 parser tests, 720-case differential. Host-only evidence.
