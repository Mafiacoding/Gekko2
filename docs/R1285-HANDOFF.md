# R1285 — software GS rasterization hot paths

Early alpha, BIOS stability first. Real Wii Sony splash/Remote evidence remains R1279/R1280. Stable hardware OSDSYS and GX drawing/performance still require owner tests.

## Changes
- Per-draw derived state identifies unmasked PSMCT32 pixels with alpha test, blend, dithering and FBA disabled. Rasterization can write complete colors directly, preserving SCANMSK and optional Z writes. The general gs_finish_pixel helper retains its independent semantics; it never trusts cached draw state.
- Simple untextured sprites with no fog and no configured Z-buffer fill swizzled rows through gs_mem_fill_psmct32_span. Scissor bounds are already applied; SCANMSK is checked on every row. No mip overrides or guest pixel cache are introduced. General states use the previous rasterization path.
- Triangle Z and fog interpolation executes only when consumed. Flat untextured triangles without depth/fog skip barycentric floating-point weights. Enabled interpolation retains its original operation order. NEVER/ALWAYS depth predicates avoid unused pure VRAM reads; comparisons keep real Z reads.
- The fill helper writes current shared VRAM in correct little-endian PSMCT32 order, sharing horizontal swizzle offsets. Capacity is the existing GS memory bounds, not an external pixel cache.

## Validation and measurements
183 native regressions pass on final source. Both linked Interpreter/JIT builds pass the inherited PPC profile/rendered texture/STQ/cache/retirement suites and 24 GX texture packing cases. Both pass 72 independent whole-VRAM byte oracles for row fills (odd X, columns/pages, zero BW/count, colors/endian and final memory bounds).

66 deterministic rendered workloads in each final build compare their entire 4 MiB GS memory against R1284: points, lines, triangles and sprites; both contexts; alpha/fail actions, blending, masks/PSMCT24, fog, depth predicates/masks, dithering, FBA, SCANMSK and a texture case. Comparisons include color and depth memory, not just screenshots. Existing oracle tests cover enabled effect semantics independently; equality to the prior renderer does not establish new hardware correctness.

Actual compiled PPC instruction counts under Unicorn (allocation/cache services mocked), isolated 126x126 opaque draw workloads:
- Triangle2247510 ->953916 (-57.56%).
- Sprite2416823 ->165581 (-93.15%).
Full per-case costs are in verification/optimizations_r1285.json. Complex states and small primitives may have setup overhead. These percentages do not describe total frame time, Wii cycles, BIOS throughput or GPU gains.

Native saved BIOS continuation advances about3.2M additional EE instructions without halt; see osd_r1285.log. This is warm host-state continuity, not Wii cold boot/menu proof.

## Installation
Rename PCSX2-Wii-R1285-Menu-JIT.dol to sd:/apps/pcsx2-wii/boot.dol. Existing BIOS/config/discs remain. GX presentation remains experimental, default OFF, Settings RIGHT toggles it. R1285 changes optimize shared software GS rendering and apply with either presentation mode. Supply sd:/pcsx2/R1285-boot.log; compare first image, OSDSYS progress, FPS and GX output if enabled.

## Remaining scope and distribution
Full EE/IOP block retirement, linking/allocator, more VU coverage and GX primitive rendering remain unfinished. The R1284 GX presentation backend is unchanged; no GPU hardware proof is claimed. Next gated GPU primitives must preserve authoritative VRAM and resolve/readback before texture aliasing or CPU reads.

Public patches/source exclude BIOS, games, private guest state and personal runtime logs; the private continuation state is separate. Claude patch/diff target the original uploaded project and are byte-verified. GitHub upload patch is cumulative against base68964c8a6ddc08fb4c470e9271cefa5e3d106394; prepared commits preserve existing history, upload remains pending.
