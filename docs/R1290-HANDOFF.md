# R1290 — experimental GX flat triangle/sprite execution

## Implemented path

Real GIF triangle and sprite rasterizers now route eligible draws to GX. The gate requires PSMCT32, untextured flat color, no Z buffer, alpha test, blending, dithering, framebuffer mask, FBA or fog. Sprites remain flat when IIP is set; Gouraud triangles are rejected. Existing scissor clipping and SCANMSK apply. Unsupported states retain the existing software path. Points and lines remain software. This is not a complete GS/GX port.

Triangle coverage uses signed 64-bit analytic edge inequalities per scanline, matching the existing inclusive integer GS software coverage. Covered row intervals become merged GX quads, so direct GX triangle edge conventions do not determine GS coverage. Sprites produce clipped rectangle spans. This path still performs CPU span generation; it does not send arbitrary original PS2 triangles directly to GX.

GX writes opaque RGB into EFB using vertex colors, no texture, blend or Z. An aligned bounding rectangle is copied to the existing deferred readback buffer. At the VRAM barrier, only covered spans are imported into swizzled PS2 PSMCT32; uncovered EFB bytes, padded edges and original VRAM alpha outside coverage are preserved. Known GS alpha is restored for covered pixels. Previous pending work resolves before the draw plan or copy buffer is reused. All R1289 CPU/transfer/checkpoint/reset/presentation/shutdown barriers remain active.

Limitations: bbox <=640x512, clipped nonnegative GS coordinates <=2047, framebuffer width multiple of 64, fully bounded destination. Triangles with original vertices outside 0..2047 fall back. GPU initialization and EFB dimensions must already be available from GX presentation; earlier boot draws remain software. Both experimental rendering and GX output are enabled with Settings RIGHT and are OFF by default. The new renderer can cost more than optimized software for small/simple draws because span generation, copy, wait and unswizzle are not batched across primitives. No FPS gain is promised.

## Verification

187/187 host-native regression tests pass. The new test covers 246 independent randomized triangle/sprite shapes, winding, clipping, SCANMSK, padding and full-VRAM byte preservation. Span coverage is checked against independent per-pixel edge equations; imports match scalar GS writes across the entire 4 MiB. Invalid plans/imports leave VRAM unchanged.

Both linked PPC ELF variants execute the GX submission path, matrix/projection work, FIFO vertex writes, queued capture and deferred masked readback. Actual GIF packets exercise ten GS-state gates for each primitive. GX/cache services are mocked; the paired-single matrix identity library routine is mocked because Unicorn lacks that instruction family. The orthographic projection and emitted vertex writes execute normally. The mock copy supplies RGB data; these tests do not simulate physical GPU rasterization, filtering, cache coherence or timings.

Inherited GS memory/bounds, GX packing and IOP JIT checks pass. With rendering OFF, all 66 software full-VRAM signatures per build remain identical to the verified R1285 baseline. No real-Wii cold boot, stable OSDSYS, GPU image or FPS result is claimed for R1290. Textures, Gouraud triangles, GS alpha/Z/masks and batching remain open.

## Install and test

PCSX2-Wii-R1290-Menu-JIT.dol -> sd:/apps/pcsx2-wii/boot.dol; ELF for Dolphin/debugging. Interpreter variants are included. Keep BIOS/config/discs. Logs: sd:/pcsx2/R1290-boot.log. In launcher Settings press RIGHT to enable experimental GX render/output; compare with OFF. Capture fresh Wii logs and images for both modes. No BIOS or game is included.

Cumulative Claude.patch/.diff apply to the original pcsx2-wii-full-debug(1).zip and are byte-verified. The checkpoint contains binaries, patches, licenses and all verification. GitHub publication remains pending.
