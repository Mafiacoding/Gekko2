# PS2Recomp → Gekko2 ABI boundary — Batch A9

Date: 2026-11-08
Branch: `codex/ps2recomp-bridge-batch-a-memory`
PR: #58

## Result

A direct link of upstream PS2Recomp-generated C++ into Gekko2 is **not ABI-safe**.
The current experiment deliberately compiles unchanged generated leaf code only
against a test-only scalar adapter. The experiment README explicitly states
that this adapter is not ABI-compatible with the upstream full runtime or
Gekko2's `ee_state_t`.

Upstream's documented generated function shape uses a RDRAM pointer and an
upstream `R5900Context*` (for example
`void func_ADDR_...(uint8_t* rdram, R5900Context* ctx)`). Gekko2 instead owns
a live `ee_state_t` with 128-bit GPR storage, HI/LO, SA, branch/delay state,
memory translation, exception scratch, TLB and device/runtime state. These
are different contracts, not interchangeable pointers.

## What was verified

- The pinned upstream is `ran-j/PS2Recomp` commit
  `e5636981a1c9de368f6b71e819c3dc188dfedbf4`.
- The unchanged generated-leaf probe is built against a test-only scalar
  adapter; the adapter is not the full upstream runtime and is not ABI
  compatible with Gekko2.
- Existing Gekko2-generated AOT is dispatched from `ee_step_budget()` and
  `ee_core_step_n()`. Its generated entries return an exact retired count
  and perform their own exactly-once interleave accounting.
- The bridge's compatibility native-window entry remains fail-closed.
- Host CI run [42492533997](https://github.com/Mafiacoding/Gekko2/actions/runs/42492533997)
  passed all ten steps after the A8 test correction.

## Integration decision

Do **not** cast or reinterpret `R5900Context*` as `ee_state_t*`, and do not
link upstream runtime services into the Wii emulator as a shortcut.

The viable route is a generated-code lowering stage that consumes a pinned
PS2Recomp output subset and emits Gekko2's existing AOT entry ABI. The lowering
must reject a function unless all of the following are proven:

1. The function's complete instruction semantics are in the supported subset.
2. Register mapping preserves all 128 bits of every architectural GPR that can
   be observed, plus HI/LO and SA where used.
3. Guest memory operations call Gekko2 memory helpers and preserve guest
   endian, TLB and fault behavior.
4. Calls, syscalls, COP0, COP1, COP2/VU, MMI, CACHE and unknown helpers either
   have an audited Gekko2 lowering or terminate the native region and resume
   through the existing interpreter/runtime.
5. The generated entry obeys the current AOT ABI: budget clamp, exact retired
   count, precise resume PC, branch-delay semantics, event boundaries and
   exactly-once `ee_core_interleave_account()`.
6. The input ELF identity, upstream commit, generated-source hash, selected
   function address and lowering version are pinned in the private build
   manifest. Generated game code remains outside the repository.

## Why this is not yet an implementation

The existing `tools/ee_aot_generate.py` translates raw MIPS words using
Gekko2's own opcode translator. It does not parse or lower upstream-generated
C++ expressions, helper calls, function dispatch or runtime wrappers. The
existing isolated probe validates a narrow scalar leaf through a test adapter;
it does not validate the full generated function ABI, MMI, memory, device
services or a callable function in the live EE scheduler.

A textual rewrite of generated C++ or a pointer cast would silently assume
semantics that are not established. No such shortcut was added.

## Next implementation gate

Build a parser/IR bridge against the exact pinned upstream output and define a
strict first subset (initially straight-line integer leaf, no memory, calls,
branches, syscalls or coprocessor operations). Lower that IR into the existing
Gekko2 AOT entry generator, then compare complete architectural state and
retirement against the interpreter. Expand one feature class at a time.

Only after host differential tests pass should the Wii cross-build, Dolphin
test and physical Wii test be attempted. No Wii compatibility or FPS claim is
made by this batch.
