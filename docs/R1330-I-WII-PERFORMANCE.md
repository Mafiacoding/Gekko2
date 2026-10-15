# Gekko2 R1330-I — Wii performance checkpoint

Source baseline: uploaded GitHub archive `71666c69917251cf61162fa304cf626c4501a093` (R1330-H scheduler/log fix).
Scope: Nintendo Wii runtime. No Dolphin/GX compatibility changes or new emulation features.

## Changes, in requested order

1. **GX synchronization.** EFB-to-texture copies now queue `GX_PixModeSync` before invalidating the texture cache and sampling. They no longer block the CPU. Real CPU readbacks, CPU texture overwrites, XFB completion before the FPS overlay/VI handoff, and shutdown still use `GX_DrawDone`. The source buffer is fenced only while a prior draw can still sample it. `GX_SYNC` reports wait counts and time-base ticks by ownership reason. A resident presentation goes from three CPU waits to one. This is a reduction in synchronization, not a claim of fully asynchronous presentation.
2. **Successor dispatch.** A 32-byte aligned descriptor per cache entry stores PC, function, length, source page/generation, mapping epoch, serial and first word. A warm continuation avoids traversing the instruction array and cold allocation data. Live word checks and generation checks remain; active chains still pin allocations against eviction. `dispatch_hits` appears in `EE_BLOCK`.
3. **Scheduler-aware native runs.** EE native chains may continue across a grant of up to eight scheduler quanta. Each block is admitted only inside the remaining eight-instruction quantum. At its exact boundary the scheduler runs one actual IOP tick before the next EE body. Scalar recovery, branch delay slots, halted EE grants and early IOP termination keep their original order. IRQ/timer retirement remains per instruction. `GEKKO2_LEGACY_SCHEDULER` retains the original IOP-driven frontend for A/B builds. The longer path uses one-tick IOP grants, which changes host IOP block grouping; it needs real-Wii performance measurement.
4. **Fused native boundaries.** Separate ALU, memory and delay helpers retire the previous instruction and perform the unchanged next-instruction proof in one native callback. The retirement sequence is inlined in those helpers to avoid an additional argument-saving frame. Preparation failure retires only the previous instruction and returns to the scalar path. The first prepared instruction and final retirement keep their existing contracts. `GEKKO2_LEGACY_BOUNDARIES` retains separate helpers. Per-instruction diagnostic counting is opt-in with `GEKKO2_COUNT_BOUNDARIES` (32-bit wrapping count); normal builds avoid that cost.

The R1330-H quantum warning remains behind `GEKKO2_TRACE_SLICE_BOUNDARIES`. Normal builds do not enable it. The Makefile now defaults to `PCSX2WII_FAST`, as do the supplied performance binaries; this enables the precise block/chain path and suppresses historical heavy diagnostics.

## Build

```sh
export DEVKITPRO=/path/to/devkitpro
export DEVKITPPC=$DEVKITPRO/devkitPPC
export PATH=$DEVKITPPC/bin:$PATH
sh tools/build_r1330i.sh
```

Toolchain used: user-supplied devkitPPC r32 / GCC 8.1.0 and supplied libogc/libfat headers/libraries. The old compiler needed a workspace-only `libmpfr.so.4` compatibility link to this host's `libmpfr.so.6`. The baseline was recompiled with the same r32 toolchain and FAST flag; comparison is not a byte-for-byte reproduction of the supplied GCC 16 R1330-H binary.

## Measured costs

Linked PPC CPU execution with mocked allocation/cache services and a two-instruction synthetic IOP callback; these are instruction counts, not Broadway cycles or FPS.

| Warm workload | R1330-H r32 FAST | R1330-I | Change |
| --- | ---: | ---: | ---: |
| Eight EE retirements: native boundary callbacks | 15 | 8 | -46.7% |
| Eight EE retirements: total PPC instructions | 3300 | 3318 | +0.55% |
| 64 EE retirements with eight IOP boundaries: native boundary callbacks | 120 | 71 | -40.8% |
| Same 64-instruction grant: total PPC instructions | 26416 | 26315 | -0.38% |
| Resident presentation: blocking CPU waits | 3 | 1 | -66.7% |

All measured CPU workloads have identical full architectural state hashes between baseline and new code. The isolated eight-instruction case slightly increases instruction count; the longer grant shows only a small reduction. Cache behavior and real IOP/GX workloads can change the hardware result. No large CPU speedup or FPS target is established by these measurements.

## Verification and limits

`Verification.json` and the checkpoint verification logs record the exact results. Key gates cover generated-PPC continuation ABI, live code/mapping invalidation, timers/IRQ at instruction boundaries, branch delay slots, vector memory, actual EE/IOP/SIF mailbox transfers, and deliberately deferred GX ownership transfers. Native host regression gates also cover the scheduler mailbox, timers, RAM guards, GX surfaces/pipeline and frontend cadence. Earlier broad integer-overflow and merge-lane oracles passed before the final frame-cost refinement; they are recorded separately from final-build gates.

The native-chain/FPU-link tests now derive private cache layout from the target ABI rather than using an obsolete fixed slot size. Focused delay tests use the current fixture directly rather than importing a historical negative-cache probe for an MMI instruction now supported by this source.

## Physical Wii test

Use the DOL as `sd:/apps/gekko2/boot.dol` (the checkpoint includes that Homebrew Channel layout). Keep the same BIOS, disc, menu choices, FPS overlay setting and cold-boot conditions as R1330-H. Compare the post-SCE/menu guest FPS and boot time. `PERF guest_vblank_mHz` is the guest rate; `presents_mHz` is frontend presentation rate. Send the generated log, especially `GX_SYNC`, `EE_BLOCK`, `PERF`, `CPU_SAMPLE` and `HOST`, and report image stability/controller response. Existing log filename conventions are retained; boot evidence identifies this as the R1330-I performance build.

Physical Wii FPS and visual correctness remain untested here. This checkpoint implements the four performance changes; it does not certify full PS2 dynarec completion or game compatibility.
