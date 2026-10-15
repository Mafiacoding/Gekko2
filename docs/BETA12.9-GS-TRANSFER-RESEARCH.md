# Gekko2 SPEED BETA12.9 — GS move + VBlank optimization

## Observed physical Wii evidence (earlier BETA12.7)

- Crash E3 obtains font assets successfully; 3 font OPEN and 5 font READ
  requests reported; the old `nufont.c(117)` NuError loop disappears.
- 3,392,525 fast VBlank skips and 1,021,812,592 event-bounded EE slots.
- 10,170 GS draws overall; last displayed buffer FBP=0 is black in the
  8x8 probe, while the active draw buffer FBP=143360 has 64/64 nonblack
  samples. The guest CPU is still in a VSync loop at 0x001FC970–0x001FC988.
- The log ends by EXIT_HBC rather than a recorded emulator crash.
- No existing per-transfer `TRXDIR=2` counters in BETA12.7; therefore the
  new GS transfer support is a compatibility fix, **not yet demonstrated**
  as the solution to Crash's black output.

## PCSX2 research and correctness decisions

- In PCSX2 `GSState::GIFRegHandlerTRXDIR`, XDIR=2 invokes `Move()` and XDIR=3
  deactivates transfers. Gekko2 BETA12.8 dropped XDIR=2 without copying.
  Reference: https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS/GSState.cpp
- PCSX2's hardware renderer invalidates texture-cache regions on memory
  transfers and tracks overlapping source/render/depth targets. We preserve
  Gekko2's existing `gs_mem` CPU/GX synchronization instead of inventing
  a guessed present-buffer switch. Reference:
  https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS/Renderers/HW/GSRendererHW.cpp
- PCSX2 2.8 notes document corrections for overbroad invalidation and
  mismatched source/destination pixel-format page layouts. We deliberately
  **decline unsupported conversions** in this build rather than copying
  incorrect colors or invalidating unrelated memory.
  Reference: https://pcsx2.net/blog/2026/pcsx2-2.8/
- Historical PCSX2 optimizer notes describe event-based EE/IOP alignment and
  careful synchronization with VU execution. There is no justified global
  skip of all BIOS/IOP spinloops.
  Reference: https://wiki.pcsx2.net/index.php/PCSX2_Documentation/PCSX2_Optimization

## Implemented

1. Parse source/destination `BITBLTBUF`, both coordinates and directions
   of `TRXPOS`, perform equal-format GS VRAM copy on `TRXDIR=2` through
   native GS swizzle/read/write functions. Respect DIRX/DIRY for overlap.
   Supports PSMCT32, PSMCT24, PSMCT16, PSMCT16S and indexed PSMs
   already modeled by Gekko2. Unsupported conversions decline safely.
2. Preserve destination alpha byte on PSMCT24 writes; do not force a flip
   or draw a stale backbuffer as if it were the guest-selected frontbuffer.
3. Add `GS_LOCAL_COPY` logs: number of successful moves, declined moves,
   source-equivalent copied pixel count.
4. Recognize an additional strictly-verified four-instruction VBlank
   wait (`LW -> ANDI 4 -> BEQZ -> NOP`) with the base register already
   holding the exact MMIO address 0x1000F000. No guest register writes,
   extra stores, or fake IRQ. Old guards/deadlines unchanged.

## Verification and remaining work

- Host GS local-copy direct memory test PASS (32-bit, overlapping right/down,
  PSMCT24 alpha, bounds/format rejection).
- Host GIF A+D register-to-copy integration test PASS.
- Extracted exact EE VBlank signature function host test PASS (MMIO address,
  live IRQ, modified branch, non-NOP delay slot and LW->SW rejection).
- Wii devkitPPC cross-build PASS with Lightning at O1 in diagnostic build;
  DOL/ELF verifier PASS and Wii MEM1 checker PASS.
- No physical Wii performance result for BETA12.9 yet. `GS_LOCAL_COPY`
  >0 proves the game exercises it; `last_fast_site` identifies a recognized
  safe waiting site. Actual gameplay and FPS require hardware evidence.
- Not implemented: local-to-host GS transfers (`XDIR=1`) and every PS2
  local-to-local format conversion. Avoid blind full-emulator speed hacks.
- Future Crash visual blocker investigations: selected `DISPFB2` updates,
  GIF path ordering, VU1/XGKICK scheduling, framebuffer alpha/blend and
  texture-cache invalidation; compare exact game operations, not frames.
