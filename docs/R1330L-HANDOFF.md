# R1330L — resident GX surfaces beta

This checkpoint starts from the reconstructed, byte-exact R1330K-B build.
It preserves the R1330J CPU/log corrections and K-B early degenerate-triangle
rejection. EE block dispatch and scheduler batching remain disabled, as in the
owner-tested K-B execution mode; the existing PPC JIT remains enabled.

## What changed

Supported textured sprites and mapped flat triangles can retain their RGB
framebuffer on GX across constant-Z tests/writes and supported GS blending.
The previous resident path accepted only opaque draws without Z writes.
The compact renderer remains the fallback for unsupported states.

| State | Resident beta handling |
| --- | --- |
| RGB output | GX rasterization and existing exact integer TEV blend programs |
| Blend coefficients | FIX 0–255, uniform source alpha, uniform destination alpha; CT24 AD=128 |
| PABE | Complementary late GX alpha passes for mixed source alpha |
| Z16, Z16S, Z24, Z32 | Exact CPU shadow; unsigned incoming Z is compared before storage truncation |
| Z test | CPU coverage planning; one contiguous passing interval per row required |
| Z write | Exact CPU writes to validated, physically disjoint depth storage |
| CT32 alpha | Exact 8-bit CPU metadata for each passing fragment |
| CT24 alpha | Preserve destination alpha bytes |
| Fragmented depth masks / aliases / unsupported alpha coefficients | Existing compact or software/hybrid fallback |

The new resident branch does **not** move the Z comparison or exact alpha storage
entirely to GX. Existing compact paths continue using native GX depth, including
their Z32 high-byte split. Alpha metadata is not an additional PS2 processor.
No VU0/VU1 or EE/IOP execution code was changed.

GPU blend destination copies cover only the aligned target rectangle and are
ordered with GX_PixModeSync before texture sampling. They do not require CPU
readback. The destination buffer grows to the resident maximum once, with a
GPU completion fence before an old sampled allocation can be freed.
Framebuffer seed and final ownership resolve still cost a full surface copy.
Source decoding, exact depth planning and alpha metadata still cost CPU work;
new source textures can require a completion fence before upload-buffer reuse.
This is a beta to measure on Wii, not evidence of a specific speed increase.

## Files and build

- `Gekko2-R1330L-GX-Surfaces.dol`: beta with the expanded resident pipeline.
- `Gekko2-R1330L-Control.dol`: same CPU mode and early degeneracy rejection;
  expanded resident blend/depth branch disabled. Old opaque residency remains.
- Matching ELF files retain symbols for diagnosis and PPC verification.
- `R1330K_to_R1330L.patch`: patch against reconstructed K source.

Use the supplied devkitPPC r32/GCC 8.1.0 and libogc 1.8.18, set `DEVKITPRO`
and `DEVKITPPC`, then run `sh tools/build_r1330l.sh`. If this older compiler
needs host shared libraries, add the local supplied `host-libs` to
`LD_LIBRARY_PATH`. The SDK is deliberately not included.

The beta uses `-DGEKKO2_GOURAUD_EARLY_DEGENERATE`. Control also uses
`-DGEKKO2_GX_RESIDENT_PIPELINE_DISABLE`. Do not silently enable
`GEKKO2_EE_BLOCK_FAST` or scheduler batching while comparing these builds.

## Verified here

- 63/63 portable GS/GIF tests pass, including the new 158-check depth/ownership
  test and K Gouraud counters.
- Real linked PPC: 40 resident GIF sprites, 10,240 RGB/alpha/Z comparisons,
  FIX/uniform AS/AD, mixed PABE, all supported Z formats and CT24 preservation.
  Every blend destination copy is the 32×16 target rectangle in that fixture;
  CPU color readback occurs only at final resolve.
- 12 fragmented-depth resident rejections follow the compact path with exact
  tested RGB/alpha/Z results.
- Existing resident oracle: 80 draws, cache reuse, overlap barriers, CT24 alpha
  and resident presentation preserving raw RGB.
- Deferred FIFO oracle: texture copies, pixel sync, sampling and CPU cache
  invalidation remain correctly ordered.
- Compact TEV oracle: 6,912 actual linked shader programs, 19,180,032 independent
  scalar comparisons, FIX factors and all A/B/D selectors; depth and alpha cases
  also pass.
- Scheduler oracle: 42 paired grants preserve the original 8:1 boundaries,
  delay slots, code/mapping updates, IRQ and halt behaviour.
- Both DOLs match their ELF load-section bytes, use entry 0x80004000, stay within
  MEM1, and retain both Wii HID4 startup signatures.

PPC tests execute linked instructions in Unicorn 2.1.4 with synthetic GX/cache
services. They do not execute Dolphin or a physical Wii GPU. Actual boot,
image correctness and wall-clock performance are **not verified here**.
Builds retain existing historical compiler warnings; they are not warning-free.

Run portable tests with `python3 tools/verify_native_gs_r1330l.py --output /tmp/r1330l-native`.
Run a linked oracle with `python3 tools/verify_gx_resident_r1330l.py <beta.elf>
--nm <devkitPPC>/bin/powerpc-eabi-nm`; ensure Unicorn is importable. Other
included linked oracles use the same ELF/nm arguments. The compact oracle
disables residency internally; the resident fallback oracle explicitly enables it.

## Owner observations and diagnostics

The newly supplied Dolphin GX log identifies itself as **R1330-J**, not K-A/B.
Its final resident-surface counters are zero. The screenshot is a black output
window with a white rectangle, without an exception message. These attachments
do not establish the cause of a K-A/B startup failure.

The K-B Wii log has 594,944 submitted Gouraud triangles, all degenerate, and
zero visible Gouraud triangles. They are not 594,944 software rasterizations.
The source/decoder should only be changed after confirming the provenance of
those vertices; moving degenerate geometry to GX will not improve its shading.

Each L BIOS log includes `BUILD checkpoint=R1330-L` and `resident_pipeline=1`
for beta or `0` for control. A fresh `sd:/pcsx2/Gekko2-startup.log` is attempted
after FAT mount and before launcher drawing. Missing/unwritable storage prevents
that file; it cannot identify crashes before filesystem initialization succeeds.
Existing `Gekko2-R1308-gx-render.log` / software log filenames remain compatible.

`GX_RESIDENT_PIPELINE` reports accepted draws, CPU depth tests/failures/writes,
GX blend draws, rejection classes, nominal compact readback bytes avoided and
GPU-only blend rectangle-copy bytes. `avoided_compact_bytes` is a comparison to
the old per-draw compact RGB capture, not a measured total bandwidth saving.
Use `GX_SURFACE` seed/resolve/present counters and `GX_SYNC` waits as well.
`ready=1` only confirms GX initialization; it does not prove a draw was accelerated.

Compare beta and control using the same BIOS, GX option and stop point. After
the same visible boot phase, pause and save a separate log for each platform
and build. Confirm the L identity first. Dolphin time-base timestamps are guest
time; use wall-clock observations for cross-platform speed comparisons.

## Primary reference code reviewed

The owner’s nullDC reference was interpreted as **nullDC4Wii**. It uses native
GX depth and alpha comparisons, and several paths employ extra GPU passes.
Wii64 also uses GX depth and alpha comparisons. This informed the design;
no renderer code was imported and their Dreamcast/N64 state rules do not replace
PS2 GS semantics.

- [nullDC4Wii gxRend.cpp, pinned revision](https://github.com/BenoitAdam/nullDC4Wii/blob/26a623e3feca9927d8ffe43706b06d82d672dd64/plugs/drkPvr/gxRend.cpp)
- [Wii64 Rice OGLRender.cpp, pinned revision](https://github.com/emukidid/Wii64/blob/a130df2aa81904ba1958e789c37eab300a7c4ce6/Rice_GX/OGLRender.cpp)
- [Wii64 glN64 OpenGL.cpp, pinned revision](https://github.com/emukidid/Wii64/blob/a130df2aa81904ba1958e789c37eab300a7c4ce6/glN64_GX/OpenGL.cpp)

The supplied libogc `include/ogc/gx.h` documents RGB8/Z24 and RGBA6/Z24 EFB
formats. RGBA6 cannot stand in for exact PS2 8-bit alpha; a 24-bit native depth
buffer cannot by itself preserve every unsigned PS2 Z32 comparison.
