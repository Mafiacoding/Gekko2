# R1295 — Wii log driven EE/GX and sprite optimizations

2026-10-04. Early alpha. R1295 builds are awaiting a real Wii run. Full EE/IOP/VU JIT and full textured GS hardware rendering remain incomplete.

## Five-hour R1294 Wii run

The supplied diskless run contains 3,315 performance samples over 18,187.447 seconds. FIRST_IMAGE is recorded at 453,108 ms, EE295,392,588, EE PC00271a74 and IOP PC00019040. The exit has EE1,745,454,691 and IOP218,558,325; neither processor is marked halted. This does not independently establish responsive OSDSYS navigation.

GX presentation initialized successfully: 10,773 output attempts, zero presentation fallbacks and zero synchronization errors. However, `GX_WORK` accepted draws, quads, readback bytes, waits and primitive attempts all remain zero. There are 545,471 software sprites and no triangles. The frequent state snapshots include textured, flat and blended sprites; `zcfg=1` alone does not establish whether depth reads/writes were active. R1295 adds the missing state telemetry rather than assuming all these sprites are eligible for GX.

Median EE throughput is about 640,094 instructions/s in the first 60 samples, but 51,210/s in the final 120. Late median core share is 98.58%; the sampled EE share is 86.5%. These samples include GS helpers invoked by EE execution. They are not a breakdown of pure CPU emulation versus rasterization. Native block runs are 167,796,911, retiring 372,405,985 instructions: an average of only 2.219 instructions per block.

## Actual rendering changes

Previously, merely configuring ZBUF excluded flat triangles and sprites from GX and excluded opaque sprites from the software row-fill fast path. A configured but masked Z buffer with testing disabled or ALWAYS now counts as inactive. Active depth tests and depth writes still use the established software path. PSMCT32/24, alpha/mask/fog/dither and blending eligibility rules remain enforced; CT24 preserves destination alpha. This unlocks additional real FIFO submissions for supported guest states without forcing BIOS registers.

Textured software sprites calculate the original U interpolation once per clipped column, reusing the identical double-precision expression across rows. Larger sprites may additionally use a 128-entry draw-local texel cache. Eligibility requires configured repeat/clamp, bounded texture dimensions, conservative physical page ranges inside VRAM, and no overlap between texture source and framebuffer or writable depth. Aliases, region clamp/repeat and unsupported ranges decline caching. The cache is disabled after each draw and never relies on a persistent VRAM generation assumption. Filtering, texture function and final GS pixel/depth operations retain their original ordering.

New periodic `GS_DETAIL` records masked depth/test function, texture format/dimensions/function/filter and dither/FBA. `GS_ROUTE` counts overlapping state reasons and cache/row-fill use across drawing calls. It does not log per pixel. These counters are process cumulative; a configured depth buffer alone is no longer treated as proof of a required fallback.

This is still a hybrid renderer: CPU coverage prepares exact spans for eligible flat GX drawing, followed by deferred masked readback. Textured, blended and complex GS operations remain software. General EE/IOP device semantics cannot be delegated to GX. GPU texture rendering and longer-lived framebuffer residency are the next major rendering work, with exact texture function, filtering, alpha and VRAM alias tests required.

## Actual JIT changes

The scalar scheduler already fetches and validates the first instruction before deciding to enter a native block. R1295 passes that fetched encoding to the block engine and, after successful native finalization and an encoding match, prepares the first instruction without fetching it a second time. The old direct block API still validates its first instruction live. Later native instruction boundaries retain mapping/source, PC/NPC, idle/halt/delay checks and the real retirement callback. Count/Compare, interrupts, timers, DMA/SIF work and budget limits are not skipped. Allocation failures cannot advance the guest. Code ownership remains valid through native return.

A shared lightweight policy header prevents scalar eligibility and native emission from diverging. Blocks now also admit SLTI/SLTIU, DADDIU, MOVZ/MOVN, MFHI/MTHI/MFLO/MTLO, MULT/MULTU/DIV/DIVU, variable 64-bit shifts and fixed 64-bit shifts including +32 variants. Only the existing nontrapping implementations are admitted. Memory/control/COP/MMI and trapping arithmetic are not silently converted into unchecked blocks. Register residency across callbacks, memory/control blocks and complete EE/IOP/VU coverage remain open.

## Validation and isolated measurements

- Both devkitPPC/libogc ELF/DOL engine pairs build.
- 193 native tests pass. The new sprite oracle compares all 4 MiB of VRAM for 32 nearest/linear, UV orientation, clamp, writable-depth and framebuffer-alias combinations. It also checks inactive-depth eligibility.
- Twelve linked-PPC jobs cover both engines: precise blocks, FIFO/routing/VRAM ownership, boot policy, 66 GS paths, profiler/input/FPS/scheduler, and IOP control/VU integration. Allocation/cache/GX platform services are mocked; emitted PPC instructions execute.
- Full 544-byte integer/HI/LO results match interpreter and R1294 across all eligible ALU families and 80 multiply/division zero, sign and overflow combinations. Source mutation, mapping replacement, interrupt positions 1–8, ABI, cache collision, allocation retry and budget exits are checked.
- All 66 full-VRAM signatures match R1294 and both R1295 engines.
- A 128x64 sprite fixture uses 936,179 -> 86,785 PPC instructions for opaque masked-depth drawing, 2,861,306 -> 2,375,399 for nearest texturing and 6,868,730 -> 4,470,739 for linear texturing. Full-VRAM hashes match. These are isolated CPU workloads with GX disabled, not Wii FPS or a uniform texture workload speedup.
- The accompanying verification logs give warmed 2/3/4/8-instruction block costs under the same fixture. Improvements target the short blocks observed on Wii. Lower mapped kernel code retains guarded scalar execution and can still have dispatch overhead; this is not a universal JIT speed guarantee.

## Wii test

Copy `PCSX2-Wii-R1295-Menu-JIT.dol` as `sd:/apps/pcsx2-wii/boot.dol`. Start diskless with the same BIOS, enable GX output and experimental GX drawing, keep the FPS overlay choice unchanged, and collect `sd:/pcsx2/R1295-gx-render.log`. Logs use a new revision filename. Compare FIRST_IMAGE, late EE/core samples, `EE_BLOCK`, `GX_WORK` and the new state/route counters. The interpreter DOL in the checkpoint is a comparison build.

Hardware acceptance requires actual nonzero GX drawing where eligible, correct BIOS pixels, no synchronization faults and improved sustained runtime. R1295 has not yet demonstrated these on Wii. Neither BIOS/disc images nor private runtime logs/checkpoints are included in deliverables.

The cumulative Claude patch/diff applies to the original `pcsx2-wii-full-debug(1).zip` tree, not on top of R1294. Patch validation and changed-source hashes are included in the checkpoint.
