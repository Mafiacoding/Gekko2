# PCSX2-Wii R1273

R1273 wires the R1272 bounded native block backend into VU0 as well as VU1. R1272 is retained as fallback. This is not completion of the full EE/IOP/VU JIT port.

## Changes

VU0 MSCAL and MSCNT select the straight-block loop only when the invocation starts with two eligible pairs. Branch-heavy entry paths retain the prior pair loop. Blocks use real vu0_vf/cop2_ctrl/vu0_acc arrays and separate 4KB data/micro memories, with cop2_ctrl[26] as TPC. The existing pending-delay guards, E/branch retirement, exact-word cache revalidation, budget clamp and unsupported fallback are shared with VU1. Codegen/cache behavior is otherwise unchanged from R1272. No RAM shadow state or interpreter callback is placed inside native blocks.

## Verification

- 180/180 native regressions.
- Both actual Wii ELF builds pass inherited EE/IOP/VU1/pair/block tests and new VU0 integration tests: actual enabled block use, TPC updates, MSCNT resume, 4KB code/data wrapping, mixed straight/branch/delay execution, unsupported fallback, pending-branch entry and code-word replacement.
- The real VIF0 MSCAL and MSCNT command parser is exercised in both ELF builds, executing and continuing the VU0 program through the actual frontend.
- VU1 block emitter semantics are unchanged; R1272's 9,760 generated block tests and finite-arithmetic accuracy scope remain the baseline.

## VU0 warm real-PPC instruction counts

Three identical samples per workload, with identical final VF/VI/ACC/local memory and expected retired counts:

| Workload | Retired pairs | R1272 | R1273 | Change |
| --- | ---: | ---: | ---: | ---: |
| Branch-heavy loop | 302 | 75,968 | 76,000 | +0.042% |
| Straight program plus E delay | 66 | 15,460 | 7,145 | -53.784% |
| Eight straight pairs, branch and delay loop | 202 | 48,068 | 27,701 | -42.371% |

This is generated PPC instruction counting in synthetic VU0 workloads, not hardware cycles, Wii FPS, BIOS response speed or a whole-emulator benchmark. Do not compare the absolute numbers with the VU1 tests as if they were identical frontends.

## Reproduce and remaining work

Build tools/build_r1273.sh with supplied devkitPPC/libogc, DEVKITPRO/DEVKITPPC and -j1. verify_ppc_vu0_r1273.py takes ELF and --nm. compare_ppc_vu0_branch_r1273.py, compare_ppc_vu0_straight_r1273.py and compare_ppc_vu0_mixed_r1273.py take before ELF, after ELF and --nm. Unicorn is required.

EE/IOP true blocks/event boundaries, block linking/register allocation, declined paths, full VU MAC/status/Q/P/EFU/random/parallel hazards and PS2 float saturation/normalization remain open. Repeated arithmetic can still expose the pre-existing PPC/host NaN-sign difference documented in R1272. No new Tekken result or physical Wii FPS claim is made.

Claude patch/diff are identical cumulative patches against original pcsx2-wii-full-debug(1).zip. Packaging checks exact patch application and byte comparison, ZIP CRC and per-entry SHA-256. BIOS/game/private RAM checkpoints are excluded.

## Authentic BIOS navigation

Native interpreter continuation from R1272: Cross -> Browser, release, Circle -> OSDSYS, release. EE 1,800,149,058 to 1,920,964,493, another 120,815,435 instructions, halt=0. Menu/Browser screenshots inspected. No guest RAM/PC/menu flags forced. This is checkpoint continuation, not a new cold boot or a generated-PPC BIOS run.
