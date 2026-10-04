# R1262: OSDSYS Browser entry and INTC delivery

## Implemented

- Positive EE kernel mask syscalls 20/21 (INTC) and 22/23 (DMAC) now
  return 1 when their BIOS handler would attempt a mask transition and 0
  if the requested state already exists. Raw register writes retain the
  controllers' toggle semantics; pending status bits are preserved.
  SLLV uses the argument's low five bits, including wrapped shift counts.
  This follows the uploaded SCPH-50004 kernel handlers at 0x800008c0,
  0x80000900, 0x80000940 and 0x80000980, inspected privately.
  No firmware bytes are included in the deliverable.
- INTC flags and Cause.IP2 become visible immediately. Exception delivery
  waits four approximate core timing ticks before a genuine, non-delay-slot
  instruction boundary. The pending delivery cancels if the line clears.
  CPU Status masking and EXL/ERL gating still apply.
- Reference: bundled primary PCSX2 R5900.cpp `cpuTestINTCInts()` schedules
  `cpuSetNextEventDelta(4)`. This implementation applies that deferred
  delivery principle to the fork's one-tick-per-instruction/idle model.
  It is not a claim of four measured hardware cycles or a complete port of
  PCSX2's event scheduler. A future accurate timeline must supersede it.
- A separate countdown replaces guest-writable COP0 Count as the delivery
  deadline. It advances during both instruction execution and parked time.
- Optional trailing EIRQ checkpoint block (two uint32 fields) preserves the
  countdown without changing EES1/EINT ABI sizes. Legacy files with END0
  alone or EEPD+END0 remain readable and start with no armed countdown.
  Duplicate, invalid-sized and impossible countdown blocks are rejected.
  Other checkpoint peripherals still have existing non-transactional-load
  limitations; do not advertise globally atomic checkpoint restoration.

## Evidence

- A new R1262 coldboot from the uploaded BIOS reaches the main menu after
  the real CROSS press/release setup sequence. Final EE=1,955,406,279,
  PC=0x0020ab40, HALT=0. The actual active GS display BP=163840 shows
  Browser and System Configuration; the orb remains visibly fragmented.
  The private RAM/checkpoint containing firmware-derived guest code is
  deliberately excluded from the deliverable.

- Changing mask-syscall returns alone did not remove the old polling stall.
- With zero-delay delivery, the real BIOS acknowledged VBLANK_START at
  0x8000040c before the OSDSYS polling loop at 0x00271150 could observe it.
- The deferred delivery lets the guest's own read/branch complete. No
  guest PC, RAM initialization flags or INTC status are changed by the
  R1262 test drivers. All navigation uses actual emulated controller buttons.
  Existing fork HLE/boot compatibility paths remain; this is not a claim
  that the entire fork is cycle-accurate or free of historical workarounds.
- Regular R1262 continuation from the fresh R1261 main-menu checkpoint:
  CROSS held for 15M EE instructions, then released for 60M. The actual
  active GS display reaches the Browser's `No data` / `Back` screen at
  EE=2,030,784,987. This is native core validation, not Dolphin/Wii hardware.
- The same controller-only Browser test starting from the new R1262
  coldboot checkpoint reaches `No data` / `Back` at EE=2,030,785,756,
  PC=0x00247700, HALT=0. Active display BP=0; see the fresh Browser
  log and display inspection. Both menu and Browser screenshots are
  active-display captures, not offscreen-buffer substitutions.
- CIRCLE then release initiates Back, but the active display stays black
  through EE=2,198,623,426. The cores continue executing. Active display
  BP=163840 has only 640 nonblack pixels; draw contexts still target BP=0.
  That buffer contains the Browser gradient, without restored menu text.
  This is a remaining Back/display-transition blocker, not a verified
  successful return to the main menu. A read-only 12M follow-up trace found
  no TRXDIR=2 commands; missing local-to-local copies are not established
  as this transition's cause. Read-only guest state at this checkpoint:
  0x001f0010=100, 0x001f0014=-1. EE is executing the tiled-background
  drawing loop around 0x00241d58, not stuck in the former VBLANK poll.

## Verification

- 168 native regressions pass. Updated full checkpoint round-trip checks
  additionally confirm a partially elapsed countdown survives save/load
  and resets for legacy snapshots.
- Each emitted Interpreter and JIT Wii ELF passes the existing 104 GS/DMA/
  metadata/STQ checks plus 27 new IRQ/syscall checks on big-endian PPC.
  The new fixture executes MIPS polling and ACK/ERET code through the
  Wii-compiled EE interpreter and tests actual syscall return/mask behavior.
  In the JIT ELF this tests the shared interpreter/interrupt path, not
  generated JIT block coverage.
- Both devkitPPC/libogc ELF and DOL variants build. Existing compiler
  warnings remain. Cumulative patch must apply byte-for-byte to the
  original Claude archive; ZIP manifests and CRC are verified.

## Next work

1. Finish Browser Back/display transition and remaining orb/GS defects.
2. Implement CDVD configuration bank/session, MMIO and RPC data/checksum
   handling and durable settings storage. Protocol is documented in
   NVRAM-CONFIG-INVESTIGATION.md; it is still not implemented.
3. Measure physical Wii throughput. gettime() uses PowerPC TimeBase;
   Dolphin requires separate host wall-clock measurement. No physical
   Wii FPS or 10-FPS guarantee follows from these native core tests.
4. Complete EE block JIT, register allocation/linking, IOP JIT and both VU
   micro-JITs with differential tests. Current JIT remains a single-
   instruction native-code cache. R1262 adds no JIT speedup claim.
5. Retest Tekken after the BIOS/core work. No new Tekken result in R1262.
