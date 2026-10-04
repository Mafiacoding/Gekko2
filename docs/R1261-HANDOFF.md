# R1261: OSDSYS controller state, filtering and clock consistency

User priorities: finish OSDSYS correctly, optimize BIOS performance toward
10 FPS if feasible, include the FULL EE/IOP/VU PowerPC JIT port, then Tekken.
No native-Wii FPS estimate or measured speedup is available. R1261 is not a
completed full JIT port. See JIT-PORT-PLAN.md for current state and gates.

## Production changes

- Append optional EEPD checkpoint block containing the legacy PADMAN and
  XPADMAN DMA addresses and status address. New checkpoints restore input
  destinations without BIOS-specific recovery. Original version-1 snapshots
  remain readable, with missing bindings zeroed rather than inherited from
  unrelated live state. This does not reconstruct bindings missing from old
  snapshots. No host file/function pointers or guest completion flags added.
- Shared STQ texture sampler implements four-tap linear filtering only when
  MMAG=1 and MMIN=1 (non-mipmapped linear in both magnification/minification).
  Its selection is independent of LOD; mixed/mipmapped modes remain pending.
  Subtract half a texel before four-tap lookup, use four fractional bits and
  apply CLAMP/repeat separately to each tap. Palette lookup/TEXA expansion
  precede interpolation. FST UV remains on the old nearest path; its fractional
  coordinate preservation and filtering need follow-up. No GIF struct ABI change.
- GS VSYNC now uses the same free-running COP0 Count-based schedule as VBLANK,
  rather than a retired-instruction count that freezes while EE is parked.
  Tests cover actual idle edges and absence of repeated spurious edges.
  This preserves the existing timing approximation, not full cycle accuracy.
  The future event timeline must be independent of guest-writable Count.
- Diagnostic emitted-VBLANK counter and Wii PERF log measure host presentations,
  guest VBLANK events, retired instructions, core time and outside-core blit time.
  Log: sd:/pcsx2/R1261-boot.log. Rates use mHz (divide by 1000 for events/sec).
  Timing uses gettime()/PowerPC TimeBase. Physical Wii measurements use hardware
  elapsed time; Dolphin requires separate host wall-clock throughput measurement.
  Neither repeated presentations nor VBLANK count proves unique rendered frames.

## Validation

- 167 native regressions pass. Targeted updated linear tests also cover PSMT8
  and PSMT4 resolved palette colors. Real PAD checkpoint round-trip, malformed
  optional block rejection and legacy format compatibility are tested.
- Both emitted PowerPC builds pass 104 checks each, including STQ triangle
  pixels and five four-tap rendered images (sprite/triangle/texel center/clamp/
  negative repeat). Shared runner checks real return with a bounded budget.
- Fresh R1261 Interpreter BIOS boot, no disk: 50,000 interleaved slices per call,
  real CROSS press/release only. Final EE=1,955,405,515, PC=0x0022eb14, HALT=0.
  Real GS active-region image displays Browser / System Configuration.
  No guest PC, RAM or initialization flags forced. This is not Dolphin validation.
- Generic checkpoint continuation preserves PAD binding state without manual
  repair. System Configuration opens (Clock Adjustment), Circle returns to
  main menu, Up selects Browser, Cross starts Browser entry. The initial
  continuation checkpoint was migrated from the documented R1259 recorded
  host-binding recovery; the fresh R1261 checkpoint includes bindings directly.

## Browser blocker still open

After Browser entry, the display fades black and EE waits in 0x00271150..168
for INTC_STAT bit 2. Another 120M retired EE instructions does not unblock it.
Snapshot EE=2,649,797,168, Count=958,656,559, Status=0x70030c11,
INTC_STAT=0, INTC_MASK=0x100e, RA=0x0022079c.
Read-only logs from a further 12M instructions show actual VBLANK followed by
BIOS acknowledgment before the polling load can observe it:
- Count 959,690,160: VBLANK at next PC 0x271150; real ACK at BIOS 0x8000040c.
- Count 964,611,648: VBLANK at next PC 0x271158; same real ACK.
- Count 969,533,136: VBLANK at next PC 0x271160; same real ACK.
The mask enables VBLANK IRQ and BIOS clears the status bit during dispatch.
Investigate why masking/dispatch/timing lets this polling routine miss every
edge. Do not inject INTC_STAT, force PC, skip the loop or disable IRQ at a
hardcoded BIOS address as a claimed correction.

## Next tasks

- Resolve Browser polling/interrupt interaction and confirm empty Browser / Back.
- Complete GS graphics (orb still fragmented), fractional UV and filter/LOD modes.
- Implement shared MMIO/RPC configuration banks and SD persistence. Provider
  checksum and block-count behavior are now verified directly from uploaded
  XCDVDMAN; see NVRAM-CONFIG-INVESTIGATION.md. No storage implementation yet.
- Then rank measured Wii bottlenecks, event/idle skipping and complete block JIT.
- Re-test Tekken afterward. No actual Dolphin or Wii hardware run is available.

The cumulative patch applies to the original Claude archive; ZIP includes
source, both engines, logs and images. Firmware, game and private RAM/execution
checkpoints are excluded. R1260 remains a separate preserved checkpoint.
