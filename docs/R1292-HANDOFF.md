# R1292 — GX CPU overhead and EE-JIT rejection reuse

2026-10-04. Early alpha. No new physical Wii boot or FPS measurement.

## Changes

GX PSMCT32 target validation now bounds the last pixel in constant time instead of scanning every pixel during preparation, capture and import. The swizzle address is strictly increasing in either coordinate for supported framebuffer widths. All size/alignment/coordinate checks and 64-bit base arithmetic remain. This removes CPU work around GPU submissions without weakening pre-write validation.

All merged row-span rectangles of one eligible flat triangle/sprite are submitted in a single GX_QUADS batch (at most 2048 vertices), preserving the previous exact GS coverage. This does not batch separate primitives: GPU readback ownership still synchronizes before a subsequent draw. Readbacks, texture packing and substantial CPU GS work remain.

The EE frontend remembers 64 deterministically rejected instruction encodings independently of guest PC. Exact full-word keys protect collisions and changed instructions. PC L0 remains the warm fast path. Reset clears both caches; allocation/init/finalization failures retry rather than becoming permanent negatives. Interpreter retirement is unchanged.

The R1291 first-image/HUD fix remains. RIGHT toggles GX output, LEFT toggles experimental primitive drawing. Drawing is opt-in and starts only after first BIOS pixels. R1292-software.log, R1292-gx-output.log and R1292-gx-render.log are separate; fresh runs may replace the preceding log of the same mode.

## Verified measurements

- 320x256 sprite preparation in linked PPC ELF: 2,879,207 -> 7,682 executed PPC instructions, about 99.73% less in this isolated step. Both plans have identical SHA-256. This excludes GPU execution and is not an overall FPS improvement.
- One unsupported EE encoding at 64 PCs: 64 -> 1 allocations; 21,184 -> 6,146 PPC instructions, about 70.99% less in this isolated workload. No general BIOS JIT hit-rate or speed claim.
- Native suite 189/189; updated EE frontend test includes transient translate-allocation retry, cross-PC rejection, SMC and reset checks.
- AddressSanitizer/UndefinedBehaviorSanitizer bounds run passes. Leak detection was disabled because the execution runtime prevents LeakSanitizer inspection.
- Bounds oracle: 6000 pixel-enumerated rectangles, plus all coordinate transitions of a 2048x2048 area for each of 32 framebuffer widths. Both PPC ELF builds additionally pass 256 randomized exhaustive address-oracle rectangles and hostile parameter tests.
- Both Interpreter/JIT linked ELFs pass GX span batching, GIF routing, masked VRAM readback, deferred ownership, bounds and boot policy tests. All 66 whole-VRAM software rendering signatures match the prior baseline. GPU/cache services are mocked in the PPC tests.

## Remaining scope and Wii test

EE/IOP/VU instructions execute on the CPU; GX is a graphics pipeline and cannot replace general guest CPU execution. Full JIT port remains incomplete. Textured/Gouraud/blended/depth/fog/dither/masked GS draws still use software. Correctness must precede expanding these paths. Repeated readback can outweigh GPU gains, so do not infer overall speed from the preparation benchmark.

No failed R1290 GX-on log was supplied; the earlier software log is not proof that GX startup is fixed on hardware. Install the JIT DOL as boot.dol, first compare GX output with drawing off. Then test opt-in drawing after a confirmed first image and return R1292-gx-render.log. No BIOS, game data or raw private logs are packaged.
