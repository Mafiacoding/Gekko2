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
- The bounded native-window wrapper delegates to
  `ee_core_native_window_begin_with_addresses()`. This existing gate performs
  live-source, scheduler/event, device, Fastmem and interleave admission and
  advances the EE retirement/PC state as part of the native-body contract.
  The wrapper is **not** a PS2Recomp executor or a general execution lease.
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
