# R1299 — exact GX fixed blending, PABE passes and extended depth

2026-10-04. Experimental alpha. Preserve R1297 and R1298 fixes. This checkpoint expands the guarded snapshot renderer; it does not complete Gouraud/textured triangle rendering, arbitrary filtering, persistent framebuffer ownership or the JIT.

## What moved to GX

All FIX coefficients 0..255 and all valid A/B/D selectors now have exact TEV programs when COLCLAMP is enabled. Source and destination colors remain in registers 0/1. Seven integer midpoint stages construct H=floor(((128-r)*B+r*A)/128), r=FIX mod 128. They use the integer sum followed by GX_CS_DIVIDE_2, not fractional TEV coefficients. The result is H-B+D, with another A-B for FIX>=128. Signed intermediate sums only enter TEV's unmasked D input. Final color clamps once. Endpoint coefficients 0/128 and A=B retain shorter programs. The longest blend program uses 13 stages; direct depth adds one, split Z32 adds two, below GX's 16-stage limit.

Uniform shaded source alpha or uniform PSMCT32 destination alpha can specialize AS/AD to FIX without changing guest ALPHA registers. Padding is included conservatively in the source scan. PSMCT24 destination alpha remains the GS-defined 128. Uniform PABE-low becomes source pass-through; uniform PABE-high becomes the ordinary blend program. Mixed PABE with a supported coefficient uses two complementary late-alpha passes: source copy for AS<128, exact blending for AS>=128. Both reuse one snapshot and one final readback. Late depth prevents alpha-rejected pixels from modifying depth.

Z16/16S now use exact hardware comparison in the Z24 buffer. Fragments above 65535 compare as 65536, preserving all predicates; the authoritative GS write preserves the original low 16 bits and untouched neighboring bytes.

Z32 with high bits can use a split hardware comparison when mixed PABE does not occupy the alpha test. The old high byte is in the depth snapshot alpha and the low 24 bits are preloaded into GX Z. The first pass accepts old_high<fragment_high with ALWAYS depth. The second accepts equality of high bytes and uses GEQUAL/GREATER on low bits. ALWAYS/NEVER need no high-byte split. Constant fragment Z uses the low 24 bits. Exact source alpha is preserved separately in the source snapshot and CT24 destination alpha remains untouched. CPU shadow resolve still checks authoritative GS depth and writes physical GS Z bytes; this release does not remove that safeguard.

GX_PIPELINE adds z32_split and pabe_split counts. GX_WORK quads now counts both hardware passes. Logs use sd:/pcsx2/R1299-gx-render.log and R1299-software.log. There remains one GX experimental option.

## Remaining CPU work and performance limits

Snapshot creation still decodes GS texture memory and applies texture function/color/filter baking on the CPU. Preparation, ownership barriers, source/destination snapshots and readback/import are also CPU work. Nonuniform AS/AD coefficients, mixed PABE plus high-bit Z32 comparison, COLCLAMP wrapping, GS alpha-test fail actions, dithering, FBA and framebuffer masks still need hybrid or software handling. Varying-Z/Gouraud/textured triangles and arbitrary bilinear phases retain software routing. Exact current coverage is inherited from R1298 and R1297.

No Wii or Dolphin R1299 performance measurement has been made. More passes can increase GPU and submission costs. A correct hardware program is not an FPS gain by itself. The R1296 owner log's overlapping 594,944 Gouraud-triangle, 654,490 texture and 642,284 blend draw counts show why triangle coverage and persistent GPU surfaces are the next major rendering tasks. The core timer includes emulation/rendering and does not isolate EE JIT from GS CPU work. Do not claim 99% GPU rendering or 10 BIOS FPS from these tests.

## Hardware precision and the 99% goal

The 32-bit limitation discussed here is GS Z32 versus GX Z24, not a blanket inability to use 32-bit RGBA textures. GX supports RGBA8 texture storage. Its RGB8_Z24 EFB has 8-bit RGB without an 8-bit destination-alpha plane; RGBA6_Z24 trades color/alpha precision for six bits per component. Exact GS destination alpha therefore needs a separate plane/shadow, not an automatic switch to RGBA6. A future mostly GPU renderer must preserve that plane and manage GS VRAM aliasing/feedback and real CPU access. GX has 16 fixed TEV stages, so some GS effects need multiple passes rather than an unrestricted shader.

Offloading most supported pixel processing is a reasonable implementation goal, but no measured 99% figure exists. CPU command/state processing, guest code, DMA, texture uploads and synchronization do not disappear. For example, if rendering accounts for 80% of total CPU time, even removing all of it gives at most 5x overall speed before other optimizations. Measure each component rather than infer CPU rendering cost from the aggregate core timer.

Primary API references: attached libogc ogc/gx.h; https://github.com/devkitPro/libogc/blob/master/gc/ogc/gx.h and https://libogc.devkitpro.org/group__tevstage.html .

## Validation

The linked-PPC texture tool exercises actual GIF and flat-triangle submissions, shader state, source/destination/depth snapshots, deferred readback and source/destination alpha preservation. Its synthetic GPU model evaluates recorded TEV RGB and alpha programs plus late alpha and depth passes independently. It does not implement or validate physical GX rasterization or timing. It evaluates 6,912 actual linked shader programs and 19,180,032 scalar color comparisons per engine: every coefficient and all 27 A/B/D selector programs, with exhaustive source/destination pairs for ordinary source-over and boundary pairs for the other selectors. Depth tests include equality, above/below high bytes, Z16/Z16S limits and mixed PABE fallbacks. Maximum stage count is checked.

Final release validation: 197/197 native tests and all eight linked-PPC jobs pass. Both engines match all 66 full-VRAM signatures. Native software GS oracle tests and previous COP1/resource/seam regressions must remain passing. Build both engines with tools/build_r1299.sh. Test the actual ELF with tools/verify_gx_texture_r1299.py and the existing flat, COP1 and full-VRAM PPC tools. GPU/cache services are mocked; owner Wii logs are required for performance claims.

## Next steps toward a mostly GPU BIOS renderer

1. Measure R1299 accepted hardware draws, remaining GS routes, snapshot bytes and readbacks on Wii. Separate EE/IOP/VU execution time, GS rasterization, texture conversion and synchronization in future profiling.
2. Add correctly gated Gouraud/textured triangles and perspective coordinates with GS color/alpha/edge/depth tests. Hardware interpolation precision and GS rounding require explicit comparison.
3. Keep framebuffer/texture data resident across compatible draws. Only resolve on genuine CPU/transfer/feedback access, after handling aliasing, formats, CLUT changes and ownership. This matters more than moving arithmetic while still copying each draw.
4. Extend dynamic coefficient, alpha-test/fail, masking and filter paths where TEV/extra passes are accurate and useful. Retain a clear fallback for states GX cannot efficiently reproduce.
5. Continue precise memory/control-flow JIT blocks independently; GX cannot execute EE, IOP or VU instructions.

Packages exclude BIOS, games, IRX files, private guest checkpoints, configuration dumps, owner runtime logs and SDK binaries. Third-party notices remain included.
