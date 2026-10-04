# R1298 — GX texture snapshots, depth and blend pipeline

2026-10-04. Experimental early alpha. This is an incremental renderer implementation, not a complete GS or JIT port.

## R1296 hardware evidence

The supplied diskless Wii log spans 1,933,317 ms and exits with both cores running. FIRST_IMAGE occurs at 445,448 ms / 295,405,556 EE instructions. GX presentation is ready with zero synchronization errors, but GX_WORK reports zero submitted draws throughout. End-of-run GS_ROUTE shows 684,113 draws, 654,490 textured, 642,284 blended and 594,944 Gouraud triangle draws; these counts overlap and are not mutually exclusive. Median core time is 96.89% of interval time; median output-copy time is 2.74%. The last twenty intervals average by median 33,055.5 EE instructions/s and 0.1965 guest VBlank events/s. Presentation rate is not guest FPS. The current log cannot establish R1298 performance.

## Implemented path

Supported sprites can snapshot a compact GS-decoded texture rectangle into GX RGBA8. Existing GS palette lookup, TEXA, texture function and source alpha are preserved. GX handles textured rectangle coverage and nearest sampling of this snapshot. Exact CPU sample indices are fitted to a ramp only when every pixel has at least a 1/8-texel margin from a sampling boundary. XY uses the R1297 fractional coverage fix. SCISSOR and SCANMSK are honored. Unsupported mappings, source feedback, oversized sources and allocation failures retain software fallback.

Nearest GS sampling is supported; bilinear GS sampling is admitted only when the existing 4-bit interpolation phase is constant along each axis. That phase is baked by the CPU into the source snapshot. Arbitrary bilinear phases, perspective/Gouraud textured triangles and mip/filter completeness are not hardware-ported. Source decoding and texture-function arithmetic remain CPU work. No persistent texture residency/cache is claimed.

Flat triangles with constant vertex Z also enter the new pipeline, using exact GS row-span coverage. Interpolated-Z and Gouraud/textured triangles retain the existing software path. The original flat opaque GX path remains available.

## Depth

Z32/Z24 values representable in 24 bits use GX depth texture preload, late GX Z comparison and constant fragment-Z replacement. NEVER, ALWAYS, GEQUAL and GREATER are mapped explicitly. Depth/color aliasing rejects the path. Z32 with high bits or Z16/16S retains exact hybrid comparison instead of truncating to GX depth precision.

On resolve, the CPU validates covered pixels against authoritative old GS depth and writes only passing pixels; requested GS Z writes preserve each format's physical bytes. This shadow comparison is still CPU work, even for hardware depth. GPU depth state does not replace GS VRAM. GPU command correctness is checked on linked PPC with synthetic GPU services; physical EFB behavior still needs Wii testing.

## Blending

Integer endpoint blend coefficients 0/128 and A=B equations, with COLCLAMP=1 and PABE disabled, use multi-stage TEV source/destination snapshots. Intermediate signed values are preserved before final clamping. CT24 destination alpha contributes 0x80 to GS blending while its physical alpha byte is preserved.

Other coefficients/equations, source/destination alpha, boosted factors, PABE and color wrapping use exact GS blend arithmetic during CPU resolve. They are implemented in the new pipeline, but are explicitly **hybrid**, not full GPU blending. Alpha-test/fail actions, dithering, FBA and framebuffer masks retain software routing.

A scalar blend bug was corrected: C division rounded negative products toward zero. GS signed modulation instead floors the signed product at >>7. The red-over-blue 50% regression now expects 127 in both channels. Primary reference is the bundled PCSX2 GSDrawScanline signed modulate16 path. This intentionally changes some old blended pixel signatures; require engine parity against the corrected oracle, not an old incorrect hash.

## Synchronization, memory and logs

The source texture is flushed, GX texture cache invalidated, GPU copy queued, GPU ownership retained until DrawDone/cache invalidation, then exact RGB/alpha/Z data imported into shared swizzled VRAM. New GS register writes do not mutate a pending pipeline snapshot. Destination/depth textures allocate only when needed and grow to actual rectangle size; bounded allocations can still fail on Wii and fall back. Readbacks and snapshot conversion can outweigh GPU savings, so no FPS claim is made.

New SD records:

- GX_TEXTURE: pipeline candidates, accepted snapshot draws, upload bytes and layout rejection counts. Flat draws using the snapshot pipeline are included.
- GX_PIPELINE: depth_hw, depth_hybrid, blend_hw, blend_hybrid. Hardware depth still has CPU shadow resolve; general blend formulas are hybrid.
- GS_BLEND: ALPHA A/B/C/D/FIX, COLCLAMP and PABE.

There remains one GX (EXPERIMENTAL) ON/OFF option. Logs use R1298-software.log or R1298-gx-render.log. Copy the JIT DOL as sd:/apps/pcsx2-wii/boot.dol.

## Validation and preserved work

Both engines retain matching 66 full-VRAM signatures and all eight linked PPC jobs pass. Native checkpoint BIOS continuation with normal controller input completes without EE/IOP halt; this is not a cold-boot hardware result. 197 native tests pass, including 324 full-VRAM comparisons covering GS depth formats, equality, ALPHA selectors/factors, PABE, clamp/wrap, SCANMSK and CT24 alpha. Linked PPC tests exercise actual GIF-to-GX calls, decoded snapshot tiles, TEV integer arithmetic, constant-phase filtering, flat triangle edges, depth setup, high-bit Z32 preservation, deferred readback and both DOL/ELF engines. GPU/cache services are mocked; no physical GPU rasterization or speed test is claimed.

R1297's guarded SLUS-20001 resource RPC, correct SPU2 sound-RAM destination, fractional sprite seams, legacy checkpoint loading and COP1 block policy are preserved. Complete memory/control JIT, full GS state coverage, arbitrary filtered/Gouraud textured triangles, batching/residency and playable Tekken remain open. Do not call R1298 a completed port. Next priority is actual Wii routing/output comparison and snapshot/readback costs, then the dominant textured/blended Gouraud triangle path.

Build: tools/build_r1298.sh with devkitPPC/libogc. Native test: tests/test_gx_pipeline_r1298.c. Linked test: tools/verify_gx_texture_r1298.py; GPU operations are mocked and TEV integer stages are independently evaluated. Private BIOS/discs/IRX/runtime checkpoints and owner logs are excluded from packages.

API references: attached libogc ogc/gx.h (GX_SetZTexture / GX_SetZCompLoc / TEV operations); Dolphin PixelShaderManager Z24 channel weights and PixelShaderGen signed TEV arithmetic: https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/VideoCommon/PixelShaderManager.cpp and https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/VideoCommon/PixelShaderGen.cpp . No fractional TEV-divide rounding assumption is used in the hardware blend subset.
