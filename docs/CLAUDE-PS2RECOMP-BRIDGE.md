# Claude implementation handoff: PS2Recomp ↔ Gekko2

Status: **implementation workstream prepared; no runtime bridge is claimed yet.**
Target branch: `codex/ps2recomp-bridge-claude-ready`.
Base: public `main` at `357c6684d12470395a57a2e0980a2325f00597ea`.

This file is the task contract for Claude Code. Read it together with:
1. `AGENTS.md`
2. `docs/CODEX_HANDOFF.md`
3. `STATUS.md`
4. `docs/PS2RECOMP-GEKKO2-BRIDGE-DESIGN.md`
5. The relevant source and tests named below.

## Objective

Implement the smallest safe, testable bridge boundary that can eventually host validated PS2Recomp-generated EE functions inside Gekko2. Do not attempt a standalone game loader, a second emulator core, a GS/GX rewrite, or a broad import of an upstream project.

The public main branch already has a mature EE interpreter/JIT/AOT runtime. The task is to integrate with its real state and service contracts, not invent parallel ones.

## Verified main-branch facts (recheck before editing)

- Canonical EE state: `include/core/ee/ee_core.h`, type `ee_state_t`.
- GPRs are 128-bit (`ee_reg128_t.ud0` low lane, `ud1` high lane); HI and LO are also 128-bit.
- Architectural fields include PC, next-PC, SA, COP0, branch-pending, 48 TLB entries, FPU registers/control, COP2/VU0 state, and other runtime state.
- `ee_state_t` already contains `gpr_generation`. This is specifically a GPR/native-residency invalidation counter; **do not silently reinterpret it as a complete state epoch**. Determine all existing mutation sites and decide whether a distinct bridge lease generation is needed.
- Existing AOT entry point: `ee_aot_try_execute_fetched(ee_state_t *st, unsigned budget, uint32_t first_word)` in `include/core/recompiler/ee_aot.h` / `source/core/recompiler/ee_aot.c`.
- AOT validates the current PC and fetched first word; generated entries call Gekko2 block-preparation helpers. This is not a PS2Recomp runtime adapter.
- Existing JIT/block guards and source/mapping invalidation live around `include/core/recompiler/ee_jit.h`, `source/core/recompiler/ee_jit.c`, and `source/core/recompiler/ppc_dynarec.c`.
- `ee_core.c` memory accessors (`ee_mem_read8/16/32/64/128`, writes) route real MMIO/device windows and TLB faults. Never bypass these with a guest-address-to-host-pointer cast.
- `ee_core_block_prepare_fetched()` currently has strict live-state assumptions (including the singleton `g_state`, PC/next-PC, halt/idle, branch and exception/TLB state). Do not loosen those guards to make an adapter pass.
- Test runner: `tests/run_test.sh`. It derives host link inputs from the current source tree. Use it; do not copy stale one-off gcc link lines from the large historical `tests/README.md`.
- Wii build: root `Makefile` requires devkitPPC and libogc. Host tests do not execute PPC machine code and do not prove Wii/Dolphin behavior.
- Existing test coverage includes `tests/test_ee_core.c`, `tests/test_ee_aot.c`, `tests/test_ee_residency_generation.c`, `tests/test_ee_jit_rejection_cache.c`, `tests/test_ee_exec_lifecycle.c`, `tests/test_ee_exceptions.c`, and `tests/test_ee_dma_bus.c`.

## Required implementation order

### Batch 0 — fresh source audit (no edits)
1. Confirm the branch base and clean working tree.
2. Re-read the complete `ee_state_t` definition and all public EE memory APIs.
3. Trace all writes to `gpr_generation`, reset, exception, checkpoint restore, TLB/mapping invalidation, and code-cache invalidation.
4. Inspect the exact current scheduler grant/retirement path and identify where a native lease could yield without skipping the per-instruction epilogue.
5. Produce a short audit table: state field → owner → mutation path → required bridge handling.

Stop and report if any required API or state ownership cannot be proven from source.

### Batch A — state adapter (host-only, no execution integration yet)
1. Add a versioned, explicitly laid-out bridge state representation. Do **not** expose raw `ee_state_t` as a stable external ABI.
2. Map the full 128-bit GPRs, HI/LO, SA, PC/next-PC, branch/delay state, and only the COP0/TLB/FPU/VU fields required by the declared supported-opcode profile.
3. Make unsupported or partially modeled state reject admission. Never silently zero unrepresented architectural state.
4. Keep GPR zero immutable at the adapter boundary and verify lane ordering with asymmetric bit patterns.
5. Use a dedicated bridge lease generation if existing counters do not cover every invalidating mutation. Do not overload `gpr_generation`.
6. Add host tests for round-trip, GPR zero, 128-bit lane order, signed/unsigned boundaries, unsupported fields and stale lease rejection.
7. Do not yet call generated PS2Recomp code from the production EE loop.

### Batch B — memory callbacks (host-only)
1. Define typed read/write callback operations for 8/16/32/64/128-bit guest accesses.
2. Route callbacks to existing Gekko2 EE memory helpers. Do not create a second RAM/MMIO decoder.
3. Preserve little-endian guest byte order, alignment, TLB faults, exception context, FIFO/MMIO side effects and code invalidation.
4. Unknown/unsupported callback operations must fail closed with a typed result.
5. Test normal RAM, aliases, boundary crossings, TLB miss/invalid/modified cases, MMIO routing, FIFO operations and writes that invalidate translated code.
6. A direct-RAM optimization is explicitly out of scope until the callback path is proven and a complete mapping/source-generation proof exists.

### Batch C — bounded execution lease (only after A+B pass)
1. Define typed exit reasons: completed, budget exhausted, event pending, interpreter required, state invalid, fault.
2. Lease budget must be no larger than the real scheduler grant.
3. Yield before event/interrupt deadlines and at every unsupported instruction, indirect control-flow ambiguity, memory callback failure, or generation mismatch.
4. Resume through Gekko2's real interpreter/scheduler path. Do not fabricate retirement counts or advance device clocks in the bridge.
5. Keep native execution opt-in and disabled by default until differential tests pass.

### Batch D — one native function pilot
1. Start with a synthetic, pure-integer leaf function and a pinned source hash.
2. Compare interpreter and generated execution from identical full architectural state.
3. Then add one memory-using function only after Batch B tests are complete.
4. Do not use private Crash E3/BIOS binaries as public fixtures. A user-provided private binary may be used only in the owner's local environment and must never be uploaded.
5. Crash E3/GT3/Tekken progression is not an acceptance criterion for a host-only pilot.

## Non-negotiable invariants

- One active owner of EE state and guest RAM at a time.
- Gekko2 owns scheduler, interrupts, exceptions, DMA, IOP, VIF/VU, GIF, GS, VRAM and GX.
- No guest virtual address is a host pointer.
- No raw `ee_state_t` cross-runtime ABI.
- No skipped per-instruction epilogue, interrupt check, timer update or HLE boundary.
- No unguarded function-pointer entry based only on ELF symbol/name.
- No code admission without source identity/hash, entry ABI, clobber/return contract, delay-slot semantics and complete reachable-control-flow coverage.
- No unknown device request treated as a successful no-op.
- No changes to existing Crash graphics/font fixes, GX alpha/fences, VIF/GIF/DMA behavior or default optimization settings unless a specific regression test proves the change is necessary.
- No BIOS, disc images, private ELF, memory-card images, RAM dumps, credentials, user logs, SDKs or generated builds committed.

## Required verification and reporting

For every batch, report:
- Exact changed files and reason.
- Host test command and actual stdout/exit status.
- Sanitizer/static-analysis results if available.
- Wii cross-build command and result if devkitPPC is available.
- Explicitly separate host-native, synthetic PPC/modelled, Dolphin and physical Wii evidence.
- Remaining unsupported state/opcodes and known failure paths.

Start with:
```sh
git status --short
git rev-parse HEAD
tests/run_test.sh test_ee_aot
tests/run_test.sh test_ee_residency_generation
tests/run_test.sh test_ee_exec_lifecycle
```

Then inspect source and propose Batch A's smallest patch. Do not jump straight to Batch C or import upstream runtime files.

## Current scope boundary

This branch is a Claude-ready implementation handoff. It does not claim that a PS2Recomp runtime, state adapter, memory bridge, native Crash port, Wii build, or gameplay path has been implemented. Preserve that distinction in commits, PR text and user-facing status.
