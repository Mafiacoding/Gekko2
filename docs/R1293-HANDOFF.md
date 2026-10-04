# R1293 — measured EE overhead, mapped reads/fetch and GX work telemetry

2026-10-04. Early alpha. Full JIT and full GS-to-GX port are not complete.

## R1292 hardware evidence supplied by owner

Both GX output and GX primitive modes reach FIRST_IMAGE with readiness=1 afterward, no logged output fallback and zero synchronization failures. Output-only: EE 295397916, ms472621; drawing mode: EE295407916, ms472362 (roughly 7m52s). Median core time accounts for about 97.17% / 96.63% of the respective sample intervals. EE consumes about 84% of sampled CPU time. Core time includes any graphics work executed within guest execution; it does not distinguish JIT arithmetic, retirement and software GS helpers.

Output-only log has disc=0, render log disc=1. The runs differ in guest/media setting and duration; their rates are not a controlled renderer A/B test. GX primitive mode being active did not establish how many draw calls it actually accepted. Physical R1293 speed is not yet measured.

## Changes

Low mapped instruction fetch uses the existing TLB/backing lookup directly, avoiding irrelevant device MMIO read dispatch. Low mapped 32-bit data reads use the same shortcut after the existing debug-watch hooks. No physical-address assumption, mapping cache, timing shortcut or new instruction-count model is added. TLB remaps, ASIDs and modified code are read live. Device literals at and above 0x10000000 retain their dispatch chain; failed translations follow existing fault semantics.

The CPU loop bypasses single-instruction LW JIT fragment dispatch. Its optimized C body is now faster in full retirement tests for both mapped and direct RAM. LW native translation remains available to direct compiler users and future block formation; other JIT paths remain enabled. JIT executed percentage is not by itself a speed or completeness measure: existing scalar and branch bodies deliberately execute inline when native fragment calls are slower.

GX_WORK diagnostics are process-cumulative counters for successful flat draws, submitted span quads, queued readback bytes, resolver waits and enabled/backend-ready draw attempts. These measure real submissions rather than the switch alone. R1293-software.log, R1293-gx-output.log and R1293-gx-render.log remain separate. RIGHT controls output and LEFT opt-in experimental drawing; first-image gating remains.

## Verification

190 native tests pass, including a corrected fixture rerun for mapped fetch. The new native test checks even/odd mapped pages, immediate TLB remapping, SMC, ASID, VPN removal, global flags, backing-store boundary and byte order. The current inherited TLB translator does not enforce the architectural V/D permissions; this optimization preserves that existing approximation. The initial fixture incorrectly expected V-bit rejection and was corrected without changing emulator permission behavior.

Both linked PPC builds pass full mapped instruction/data retirement, Count/PC preservation and live remap/SMC. Eight mapped ADDIU: 5777 -> 4321 PPC instructions (~25.20% less). Eight mapped LW with JIT enabled: 9338 -> 5209 (~44.22% less). These are isolated host instruction counts with mocked platform services, not Wii cycle or FPS predictions. On the intermediate build the standalone LW JIT fragment was slower than inline C in mapped and direct memory tests, motivating its dispatch bypass.

Both builds pass GX batching/routing/readback/ownership and 64-bit work-counter checks, GS bounds, boot policy, Remote/FPS/cadence/scheduler checks. All 66 full VRAM signatures match R1292. Physical GPU services are mocked in ELF tests.

## Remaining priorities

EE full block retirement/integration, linking and register residency remain required; IOP/VU coverage is incomplete. Interrupts, timer edges, SIF operations and branch delay slots must remain correct when forming blocks. Textured/Gouraud/blended/depth/fog/dither/masked GS states still use software. GX copies/readbacks still synchronize per primitive, so copying everything blindly can cost more CPU/GPU time. The new work counters should inform texture/render-target residency and batching work.

## Install and compare

Use PCSX2-Wii-R1293-Menu-JIT.dol as boot.dol. Repeat the previous BIOS/media configuration with GX output; test experimental drawing separately. Return the corresponding R1293 logs. A real hardware timing comparison is required before claiming higher BIOS FPS. No BIOS/game contents or raw uploaded logs are included in the checkpoint.
