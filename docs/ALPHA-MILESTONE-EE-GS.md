# Alpha milestone: EE execution windows and general GS coverage

Owner direction, 19 October 2026: withhold another release/download until a
substantial, verified improvement is ready. This branch does not bump the
version, publish a DOL or claim an alpha milestone has been reached.
Baseline: BETA12.33 source `778ec77`, main still `ca16340` when checked.

## Implemented first work

**EE stable masked levels.** Native event windows previously rejected any pending
INTC/DMAC level, even when COP0 prevented exception delivery. Stable pending
levels can now admit otherwise eligible windows while publishing Cause.IP2/IP3.
INTC delay, initial assertion and deassertion transitions remain precise; a
pending unmasked deliverable interrupt still rejects the window. Existing
code/mapping/RAM, DMA/VU/GIF, timer/Compare and interleave checks remain. This
uses the existing experimental native-events option, factory OFF; no default,
clock ratio, window ceiling or saved setting changes. Quiet-retirement stubs
are unchanged. Admission alone does not prove faster executed PPC or Wii FPS.

**Textured GS points.** The POINT rasterizer previously ignored PRIM.TME and
rendered vertex color. It now samples UV or ST/Q coordinates through the shared
texture sampler, then applies the shared TFX/TCC/TEXA pipeline before fog and
pixel tests. This applies by GS state to any title, without a game-name switch.
It is a software-GS coverage correction, not a newly accelerated native GX path.
Line texturing, AA1 coverage and precise subpixel rules remain separate gaps.

The PCSX2 software reference is `GS/Renderers/SW/GSRasterizer.cpp::DrawPoint`,
which feeds the complete vertex through SetupPrim/DrawScanline, rather than a
color-only write; the shared texture stage is in GSDrawScanline.cpp. Existing
vendored GPL reference sources were inspected; no upstream backend was imported.

## Executed checks

- `tools/build_host_core.py test_ee_masked_window`, then the printed executable:
  PASS **1,557** admission/Cause/Count/retirement/timer/pure-decline checks.
  The same test compiled against the previous EE source FAILS on masked DMAC
  admission. Tests cover global interrupt enable, IM2/IM3, EXL/ERL, both levels,
  INTC armed/delay states and 8/16/32-slot requests. This is a white-box admission
  oracle, not full native-body/PPC execution or full device-order equivalence.
- `tools/build_host_core.py test_gs_textured_point`, then the executable:
  PASS **8** UV/STQ, CT32/24/16/16S and TEXA cases. The initial fixture mistakenly
  used word-addressed texture storage with a block-addressed TEX0 BP; corrected
  the fixture's units before obtaining these results.
- Selected existing tests: EE idle scheduler (zero failures), **30,000** mixed
  timer transactions plus wrap/mutable-state cases, and **32,768** GS blend/
  dither cases (zero failures).
- devkitPPC r50/GCC16.1.0 `-fsyntax-only`, GEKKO/HW_RVL: EE and GIF translation
  units checked with libogc3.1.0 headers. This is **not** a linked DOL/ELF or
  MEM1 gate. No new 83/21-suite total, Dolphin, physical Wii or game run claimed.

## Required larger milestone before distribution

| Work | Acceptance gate |
| --- | --- |
| EE execution window throughput | Execute paired linked PPC warm and cold workloads, including masked levels, timer/IRQ arrival, SIF, DMA invalidation and VU barriers; retain canonical full-state equivalence. Measure total workload cost, not just admitted-window count. |
| Avoid cold compilation regressions | Attribute formation, event variants, prefixes and cache replacement separately. BETA12.33 already shows that a warm gain can coexist with substantially worse cold cost. |
| General GS coverage | Extend format/alpha/primitive matrices, textured lines and AA1 only against independent reference behavior; retain safe software/GX ownership barriers. |
| BIOS squares | Match BIOS, NVM/card state, animation phase and captured GIF input. Locate expected square-producing draws, then compare geometry, sampled RGB/alpha, Z/DATE and scanout. A reference stream without those draws cannot prove a color-depth defect. |
| Alpha support | Matched BIOS/OSDSYS and actual game progression checks, responsive input/menu/cold boot, known limitations and stable physical GX. Automated host/PPC fixtures run first; existing owner evidence is reused. |

All 13 texture formats and four color target formats already have partial
implementations/tests; enabling experimental cross-depth conversion globally
is not a correction for missing geometry. A color-depth fault is established
only when the same packet/VRAM input produces a wrong decoded texel or target
pixel. No artificial tiles, brighter global output or bypassed IRQ/fence is
introduced. No claim that the new POINT path fixes the photographed BIOS.
