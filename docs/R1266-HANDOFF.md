# R1266: OSDSYS UV filtering and four more EE JIT instructions

## Verified changes

The OSDSYS postprocessing sprites use UV coordinates (FST=1). The renderer
previously excluded that coordinate mode from TEX1 bilinear filtering even
when both MMAG and non-mipmapped MMIN requested linear filtering. Removing
the coordinate-mode gate gives UV and STQ the same four-tap lookup. In the four matching frames, 37,452–37,677 pixels change. Four
matching main-menu frames no longer show the pronounced vertical orb
stripes. This is a real renderer change, not an image edit or guest-state
patch. Mixed filters, mipmapping, fractional UV decoding and further GS
accuracy remain separate work; this is not a claim of perfect GS emulation.

GIF PRE now changes PRIM only for nonempty PACKED tags. REGLIST, IMAGE and
empty tags preserve primitive assembly. The bundled primary reference is
`docs/reference/pcsx2/pcsx2/GS/GSState.cpp`. This independent correctness fix
changed zero pixels in its two-frame BIOS comparison; it was not the cause
of the orb stripes.

The PowerPC EE translator now emits PLZCW and unsigned saturating PADDUW,
PADDUH and PADDUB. PLZCW counts redundant leading sign bits of the two low
32-bit lanes and preserves the upper 64 bits. Saturating additions process
all 128 bits and support source/destination aliases and register zero.
The interpreter also incorrectly wrapped these three additions; it now
clamps overflow to the lane maximum, as the bundled primary MMI.cpp does.
No timer cadence, interrupt handling or guest boot flags are bypassed.

## Validation

- 177/177 native regression executables pass, including genuine EE saturating
  instructions, GIF PRE rules and actual UV sprite pixels.
- Each emitted Wii ELF passes 188 shared-core checks under big-endian PowerPC
  Unicorn. The two new checks render nearest and bilinear UV sprites through
  the real compiled GIF entry point. These are distinct from JIT block tests.
- 2,200 PLZCW and 7,200 saturating-add generated PPC blocks pass independent
  numerical oracles under Unicorn, checking the full register state.
- Authentic BIOS checkpoint continuation renders four main-menu frames with
  PAD released and HALT=0, reaching EE 1,497,516,706 / PC 0x0027173c. Instruction
  counts and PCs match the pre-filter comparison.
- Authentic saved 16:9 configuration renders two further frames without halt,
  reaching EE 1,354,098,494 / PC 0x002320a8.

Drivers are in tools/verify_osdsys_render_r1266.c and
 tools/verify_osdsys_navigation_r1266.c. Runtime checkpoints are private because
 they contain BIOS RAM; none are included in the public archive. Main-menu
 and settings validation here resumes authentic earlier boot states. R1265's
 fresh diskless boot and configuration-file readback evidence remains included
 in its handoff; a new full cold boot was not claimed for this round.

Browser navigation is also verified through real PAD input: CROSS then release
reaches the No data page at EE 1,150,364,612 / PC 0x0024aa54; CIRCLE then
release returns to the main menu at EE 1,195,858,688 / PC 0x0022e134. All
four input stages have HALT=0. No guest PC or RAM flags were changed.

## Build and use

Run tools/build_r1266.sh with DEVKITPRO and DEVKITPPC set to your toolchain.
The Interpreter ELF/DOL is the recommended BIOS regression baseline. The JIT
ELF/DOL is experimental. Keep BIOS paths and SD configuration from R1265;
`sd:/pcsx2/bios-config.bin` stores settings. Do not ship a BIOS with this project.

## Remaining work

The complete EE/IOP/VU JIT port is not finished. Four new instruction emitters
are incremental progress, not a full port. Block-level execution must preserve
interrupt/timer/RPC cadence; the existing single-instruction cache is still
an important performance limitation. No physical Wii or Dolphin measurement
was available, so no 5/10 FPS or speedup is asserted. Bilinear filtering adds
texture reads and may cost rendering time despite improving image quality.
Tekken was not rerun in this round. Prior Namco-logo progress does not imply
gameplay readiness. Next priority is further OSDSYS correctness and profiling,
then validated multi-instruction JIT dispatch and remaining EE/IOP/VU coverage.
