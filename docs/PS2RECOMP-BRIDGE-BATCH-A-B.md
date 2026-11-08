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
