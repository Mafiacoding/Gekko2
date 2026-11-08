# PS2Recomp Bridge — Batch A/B initial implementation

Status: integration scaffold only; not a PS2Recomp runtime integration.

## Included
- Versioned ABI handle with opaque backing reference to canonical Gekko2 EE state.
- Handle magic, ABI version, and structure-size validation.
- 8/16/32/64/128-bit guest-memory callbacks using the existing ee_mem helpers.
- Read outputs remain unchanged when the EE memory helper raises a TLB/EE fault.
- Host regression source for little-endian RAM, 128-bit roundtrip, bad handles,
  and KUSEG TLB-miss reporting.

## Ownership and boundaries
The adapter does not copy or redefine the EE register file. ee_state_t remains
canonical and private behind the handle. Guest addresses are never converted
to host pointers by this bridge. TLB, scratchpad, VU windows, BIOS, MMIO, and
memory dispatch remain owned by Gekko2.

The adapter does not execute native functions, alter the scheduler, retire
instructions, or perform an EE epilogue. A future caller must establish a
valid Gekko2 execution boundary before invoking it. A successful memory
callback is not proof that native execution is safe.

## Limitations
This is an in-process C ABI for the current build, not a stable cross-version,
cross-compiler, save-state, or external-plugin ABI. The bridge preserves
existing memory-helper semantics, including zero/no-op behavior for some
unmapped KSEG backing-store accesses. TLB faults raised by existing helpers
are surfaced as EE_RECOMP_BRIDGE_GUEST_FAULT.

## Validation
Run: tests/run_test.sh test_ps2recomp_bridge

This patch has not been compiled or executed here. Do not mark the test passed
until CI or a local run succeeds. No Wii build, Dolphin run, physical Wii test,
FPS, graphics, or gameplay claim is included.


## Batch C — bounded native-window admission

The bridge now exposes `ee_recomp_bridge_begin_native_window()`. It is an
admission boundary, not a code executor. It validates the bridge handle and
clean transient EE state, checks the expected PC/pipeline shape, then delegates
to Gekko2's existing `ee_core_native_window_begin_with_addresses()` gate.

The existing gate remains the sole authority for native-window admission:
feature toggles, live instruction/source checks, event deadlines, timer/VBLANK
boundaries, DMA/GIF/VU/SIF/INTC state, fastmem proofs, and EE/IOP interleave
limits remain owned by Gekko2. The bridge does not duplicate scheduler policy.

Contract for a future PS2Recomp caller:
- Pass the canonical Gekko2 EE state through the bound bridge.
- Supply a bounded, live instruction window and the register dependency mask.
- A return value of zero means decline; execute through the precise path.
- A nonzero return is the exact admitted instruction count. Execute only the
  matching native body immediately; do not retire or advance those instructions
  a second time.
- Do not yield, call guest helpers, or re-enter the scheduler between admission
  and completion of that native body.
- Memory-bearing windows remain subject to the existing Fastmem proof path.
  This API does not authorize arbitrary guest-address-to-host-pointer casts.

The host regression test verifies the detached/local EE-state case fails closed
without advancing its PC or retirement count. It does not claim successful
native execution: the existing scheduler gate requires Gekko2's live singleton
EE state and runtime device state, so a standalone host fixture must be refused.

## Host test workflow

The PR's GitHub Actions workflow runs
`sh tests/run_test.sh test_ps2recomp_bridge`. It supplies the vendored header
include paths and function-section garbage collection required by the repository's
source-glob host runner. This test is a host regression only, not a Wii build or
hardware/gameplay validation.


## Synthetic ELF / real-core pilot

The isolated ELF control-flow compiler and synthetic execution harness now live
under `experiments/ps2recomp-wii/`. The integration harness binds this bridge
to the live EE singleton, seeds synthetic code/data through bridge memory
callbacks, and executes generated Gekko2 AOT code with the existing interpreter
fallback. The bridge is therefore exercised in the same host process as the
real Gekko2 EE runtime.

Latest bridge-integrated host CI: run `42485350273`, commit
`ce33dd23e4d23c92b4703528549f83a33fef2413` — PASS. It reports 512/512
interpreter/AOT comparisons, 512 AOT hits, and 5 compiler/CFG unit tests.

Scope remains deliberately limited: generated code is emitted by Gekko2's
existing guarded AOT generator from a synthetic ELF. This is not the external
PS2Recomp runtime, not a Crash function, and not Wii/Dolphin/hardware evidence.
