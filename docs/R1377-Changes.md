# Gekko2 R1377 Graphics Beta

## Proven raster fix

The old CPU triangle rasterizer and GX row-span preparation included all three boundaries. Adjacent triangles could both write the shared diagonal and double blend its pixels. R1377 applies a common winding-normalized top/left inclusion rule to CPU coverage, CPU-generated GX source snapshots and GX row spans. This fixes integer-coordinate shared-edge coverage; it is not a full 12.4 subpixel GS rewrite.

A targeted reference test renders a rectangle using two additive triangles in both diagonal directions and both windings. The R1376 CPU gives shared-edge RGB=303030 instead of 181818; R1377 gives 181818 and has no coverage gaps or duplicate pixels. The test configures the actual stored GS context as well as active fields.

PCSX2 reference inspected: GSRasterizer.cpp DrawTriangleSection uses a top<bottom scan loop and a right-left pixel count after ceiling edge positions. No PCSX2 implementation was copied.
https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS/Renderers/SW/GSRasterizer.cpp

## Dark BIOS diagnosis

A new GS_SOURCE_RGB line samples an 8x8 grid in CPU-visible scanout VRAM: samples, average RGB, maximum approximate luminance and nonblack count. It can expose dark source colors before display conversion. It does not measure GX output pixels or television/camera brightness. Reads use ordinary VRAM coherence and can resolve a pending resident target.

No arbitrary gamma boost, global alpha change or texture-color hack was added. The submitted photos show dark surfaces and streaks, but do not establish their exact origin. Full correction of the photographed objects/darkness and physical Wii freeze stability remain unconfirmed.

## Retained changes and variants

R1376 sparse 4x4 tile seeding and R1375 dirty-rectangle readback remain. Existing JIT, DSI fastmem and Lightrec pilot remain. ARM behavior unchanged.
A disables resident blending; B enables bounded additive blending and the resident completion fence. Both include corrected coverage and source-color logging. No frequent SD dump writer.

## Verification

44 native programs passed, including corrected context setup for 19,584 CPU/GX source-shading parity pixels, shared-edge coverage, full framebuffer/padding/edge roundtrips, source RGB probe, alpha/depth, cache, VU and game-service regressions. Linked PPC tests with GX mocked passed A blend rejection and B sparse seeding, repeated tile preservation, partial readback and fences. Both DOL files match ELF sections and fit MEM1.

No physical Wii or Dolphin execution here. GPU raster/TEV pixels, FPS and all photographed object defects are not validated by mocked GX calls.

## Test

Save the ongoing R1376 log before restarting. Cold boot R1377 B using the same settings for a visual comparison. A is included as a CPU-blending control if needed. Logs use R1377; BUILD identifies A/B. Compare shared-edge streaks and dark objects and retain GS_SOURCE_RGB and GX_SPARSE_SEED lines.
