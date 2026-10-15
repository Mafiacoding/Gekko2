# R1348 Beta: native EE windows, proven RAM loads, VIF payload repair

## CPU scope

A new option, `EE native event windows (beta)`, is OFF by default so previous
settings remain a usable control. It additionally requires EE dynarec, EE
native blocks and CPU inactive service guards. RAM-load windows require Fastmem.
Settings save as version6; versions1..5 remain readable. A version5 file must
not contain the new bit24. No previous bit changes identity.

Eligible bodies use supported nonfaulting scalar integer operations and
read-only scalar RAM loads. ADD/ADDI/DADD with overflow exceptions, branches,
stores, COP0/COP2, FPU/MMI, MMIO and device helpers retain the existing routes.
This is targeted hot-path work, not a completed native implementation of the
entire emulator. C source already compiles to PPC machine code; the change
removes repeated callback/dispatch work, not the C language itself.

An independently allocated owner-bound native variant performs one admission
callback for the exact remaining grant2..8, then executes emitted PPC without
per-instruction prepare/commit callbacks. This also handles short grants from
an existing longer owner; it does not allocate a prefix for an admitted window.
Existing write-through residency keeps repeated GPR sources in PPC registers
without checking their generation again between these helper-free bodies.
Canonical guest stores remain visible at every return. Source, layout and
owner lifetime use the existing cache proofs. Rejected opcode families cache
their classification for that owner; changing owner words resets it. Translation
sampling includes native variants in GP_COMPILE and BLOCK_CACHE.

Admission rejects pending INTC/DMAC lines, armed/delayed INTC delivery, pending
SIF/RPC/CDVD service work, timer boundary crossings, VBLANK/VSYNC phase crossings,
Count wrap or a possible enabled Count IRQ. Special BIOS hook PCs and legacy
Browser injection are excluded. Every admitted source word is checked live.
The host grant is clamped to the existing eight-EE-slot/one-IOP-tick boundary.
Only after all proofs succeed are Count, retired count, display clock, deferred
peripheral timer ticks, Cause levels and the sequential pipeline published for
that grant. No memory/device operation inside this window observes those
intermediate values. All rejected cases leave guest clocks/pipeline unchanged
and use the existing precise route. Hardware events are never postponed across
an admitted event boundary or IOP handoff.

For scalar RAM loads, every address is pre-resolved with current V/D/alignment,
backing-range and MMIO/scratchpad rules. A base register modified earlier in
the proposed window declines the whole window. The emitted load consumes the
proven physical offset directly; no per-load C translation call remains inside
the admitted body. No RAM stores or self-modifying code are batched here.

EE_NATIVE_WINDOW logs enabled/runs/retired/probes. A growing retired count proves
that the new execution route is actually used. A checkbox alone does not.

## VIF geometry-source repair

The old V3 UNPACK size logic confused the extra W-component lookahead with
consumed payload. V3-32 requires12 bytes per vector, not16. The old handler
could swallow the next VIFcode or wait for bytes belonging to that command,
misaligning the geometry/VU command stream. Payload sizing now uses the actual
source group size; W lookahead no longer advances the command cursor.
Local primary reference: docs/reference/pcsx2/pcsx2/Vif_Unpack.cpp,
vifUnpackSetup and UNPACK_V4. Existing cycle stepping and W read semantics stay.
Unavailable W lookahead reads zero; this does not implement all QW-alignment
quirks for that undefined/overread lane. The XYZ source data is preserved.

96 actual linked-handler cases check VIF0/1, V3-32/16/8, vector counts1,2,3,5,
whole/split transfers, XYZ values and the following STROW command. This proves
the stream correction. It does NOT establish that this was the cause of every
collapsed BIOS triangle in session21. Four bounded GS_VERTEX_SOURCE rows record
path, encoding(0PACKED/1natural A+D/REGLIST), current EE PC, raw expanded XYZ,
XYOFFSET and drawing kick. They help distinguish zero incoming positions from
an assembly/offset problem. There is no fabricated geometry or forced drawing
of degenerate triangles. Physical BIOS geometry still needs a fresh test.

## ARM startup repair and limits

Session21 requested ARM/IOS222 but remained on IOS58 with loader-3: /dev/mload
could not open. Previously IOS selection only ran before launcher settings were
changed. Newly saved choices can now select installed nonstub IOS222 before
FIRST guest boot, while no BIOS/disc owners or worker jobs exist. An existing
session still requires a full app restart. IOS is never installed or patched.
ARM_STARTUP logs requested/active IOS and whether restart is required.
Worker startup failure now increments errors. ARM_TRANSFER operation5 records
the signed /dev/mload open result rather than a misleading zero transport result.

Linked PPC mocks verify successful selection, absent/stub refusal, selection
only with both explicit ARM+IOS flags, live-session refusal, and recovery of
IOS/storage/input after MLOAD, reload or SD-remount failures. This does not prove
activation on the physical Wii. The separately supplied ARM ELF is unchanged.
A compatible installed /dev/mload service is still required; IOS58 cannot gain
that service merely by enabling an emulator option. BIOS session21 contains no
CSC jobs and no RAM-helper jobs, so ARM availability alone is not a BIOS speedup.

## Verification and test

The frozen final ELF must pass the35 linked-PPC regression groups before
packaging. Additional native-window comparisons cover complete EE, timer,
IRQ/display state, arithmetic and persistent GPR sources, RAM loads and grants2..8.
Seven pure declines cover timer/VBLANK/Count IRQ/INTC/source changes/base changes/MMIO.
Warm 100-block arithmetic fixture cost is reported in the supplied logs; it is
not an estimate of physical Wii FPS. Scheduler, prefix, cache pressure, VU routes
and first-entry tests are also retained. Platform services are mocked.

Exit to HBC, replace DOL, restart. Keep Cache Reuse ON and EE Compact OFF.
First comparison: same R1347 options, new native-window option OFF.
Second comparison: native-window option ON and CPU inactive service guards ON;
keep all other settings identical. For the RAM test use Fastmem ON in BOTH
control and native runs. Cold Boot/New Log each time. Compare FIRST_IMAGE time,
PERF, TIME_SAMPLE and EE_NATIVE_WINDOW retired at the same BIOS phase.
Test ARM separately with saved ARM worker + ARM startup IOS enabled. Confirm
active_ios222, MLOAD availability and actual jobs; include the startup log.
