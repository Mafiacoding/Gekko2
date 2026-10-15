# R1353 Beta — event interleave, source proofs, long blocks and IOP HLE pilot

## Physical baseline

R1352 Wii log29 reached FIRST_IMAGE at418915ms (6:58.915), versus R1351C415557ms. ARM handshake succeeded: IOS222, magic474b4131, available1, but submitted/completed0 in BIOS. DSI had86 faults/patches and18095333 preparations. All263424 Gouraud triangles remained degenerate; path index1 is actual PATH2/VIF DIRECT. R1353 does not claim those triangles repaired or ARM BIOS execution.

## Implemented changes

1. Optional **EE / IOP event batches (beta)** changes interleave from eight EE slots per immediate IOP tick to windows of8..64 slots and corresponding IOP catch-up. Timer/Count/VBlank distances and already pending EE/IOP work reduce admission. SIF mailbox access, SIF DMA register access and cross-core HLE RAM adapters synchronize earned IOP ticks before exposing shared state. Device IRQ/MMIO/syscall notifications request a return to fine interleave at the next8-slot boundary. Recursive IOP accesses cannot re-enter the EE. Retirement deltas include in-flight precise blocks, while explicit accounting preserves parked/fault slots. This remains the project's approximate instruction-slot8:1 model, **not** a cycle-accurate PCSX2 scheduler; the temporal ordering is experimentally different.
2. Optional **IOP verified idle batches (beta)** advances only an already parked core, without fabricating retired instructions, strictly before timer/VBlank/thread deadlines. Pending IRQ, async I/O, load/branch hazards, exposed/invalid thread hints or dense timer events keep the original ticks. It does **not** skip the reported0x19064 busy loop simply by recognizing its PC.
3. Optional **EE long blocks (32 instructions)** expands formation, precise emitters, budget exit tables, pure event-window proofs and residency plans to32 words. A truncated short owner can grow in place when a larger grant arrives; guarded links, prefixes, source/mapping invalidation and executing-code ownership are retained. IOP catch-up deliberately uses warmed one-tick native owners for running code to avoid generating a new family of remainder-sized IOP variants. Parked cores use the actual bulk idle path.
4. Existing **EE native event windows** additionally validate their live pinned owner/source and mapping generations once. Immutable candidate metadata then avoids repeated instruction fetching within a helper-free, read-only RAM window. Unproven ownership falls back to the full word-by-word proof. Stores, MMIO and unsupported bodies still use precise/scalar paths. Actual CPU writes and mapping/source invalidation are tested against warmed windows.
5. Optional **IOP SIO2 service HLE (beta)** registers actual loaded `sio2man` exports4..20 and replaces only their documented register/FIFO ABI. It validates source bytes, indexed arguments and pipeline state; disabled, changed or unhandled calls retain the original IOP code. No imports are rewritten. Memory-card/Pad wire handling reuses the existing SIO2 device model. It does **not** replace complete CDVD, audio, Memory Card or IOP kernel modules. Native HLE is an accuracy/timing experiment, not a promise of zero IOP cost.
6. EE timer MMIO helpers now find a real timer address before materializing or invalidating deferred counters. Failed probes no longer flush timer bookkeeping. The IOP VBlank phase advances through a cached32-bit phase on consecutive ticks and reconstructs itself after tick jumps/wrap/restore instead of taking a64-bit remainder on every tick.
7. Log rows `EVENT_SCHEDULER`, `IOP_IDLE_BATCH`, `IOP_SERVICE_HLE` and `EE_LONG_BLOCKS` report actual activity. Options format7 retains old version6 settings; new switches are OFF until explicitly enabled. Pause/Resume does not apply queued CPU options; use Cold Boot.

## Deliberately unfinished requests

A persistent **Wii BIOS checkpoint/quick boot is not implemented**. The existing checkpoint engine is a host survey tool: it stages approximately40MiB of duplicate RAM/VRAM, contains process-local state, and applies some peripheral blocks before complete validation. Exposing it in the Wii launcher would not satisfy safe and complete restore. Existing host checkpoint tools remain in the source; they are not a Wii startup feature. Full event/cycle scheduling, arbitrary busy-loop proof, full IOP service replacement and repaired PATH2 geometry remain separate work. Do not report all five proposals fully completed.

## Verification

Actual linked PPC execution under Unicorn, with platform cache/MMU services modeled where necessary:

-26 complete EE/IOP/timer state comparisons for independent programs and6 shared SIF mailbox grants; exact tick and retirement ratios while cores run.
-45 complete long interpreter/precise/event-window states, grants9..32.
-154 previous dependent EE-window state cases and7 pure declines.
-84 parked IOP/timer state cases including VBlank start/end, timer IRQ and pending interrupt wake; no synthetic retirement.
-23 registered SIO2 ABI calls,3 pure fallbacks and actual IOP scheduler routing.
-Warm source windows observe actual CPU code writes, restored code bytes, mapping epochs and source generations.
-12 mixed prefixes/arena-pressure cases,40 branch/delay budget edges, DSI scalar/block/fault restoration suite,30 generated-helper plus68 scalar-memory EABI cases.
-30000 EE and30000 IOP timer transactions; native system handshake; option persistence/version migration.
-DOL section bytes match ELF and Wii startup/entry/load bounds pass. These checks are not a Dolphin or physical-Wii boot test.

Synthetic warm scheduler comparison: same independent32-ALU EE loop and3-instruction IOP loop,512 warm-up slices then256 measured slices:

| Configuration | Executed PPC instructions |
| --- | ---: |
| R1352 reference |983300 |
| R1353 new batch switches OFF |816306 |
| R1353 event batching only |1045147 |
| R1353 event + long + parked idle |909656 |

The proven source/service reductions improve this particular baseline by about17% with new batching OFF, or about7.5% with all three batch switches ON. Batching alone is worse here, and cold translation can cost more. This is deliberately reported rather than hidden: **no doubled throughput or Wii FPS gain is established**. BIOS workloads may differ and must be measured. Dependent window instruction counts also improve, but do not translate directly into total-emulator speed.

## Wii test

Install apps/Gekko2-R1353/boot.dol. The R1352 ARM worker remains wire-compatible; no IOS install/change is needed for this comparison.

T1: keep the existing R1352 settings; leave the four new switches OFF; Cold Boot BIOS and capture FIRST_IMAGE.
T2: enable `EE / IOP event batches`, `IOP verified idle batches`, `EE long blocks`; keep `IOP SIO2 service HLE` OFF; save, Cold Boot, capture first image with the same BIOS/settings. Check actual EVENT_SCHEDULER/EE_NATIVE_WINDOW activity.
T3: optional Tekken comparison with `IOP SIO2 service HLE` enabled, then Cold Boot; inspect registered/calls/fallback counts, input and boot progress.

Presets are examples based on the user's R1352 mask01fd7fff. Preserve your existing optimization.cfg or use the menu; do not overwrite it automatically. Pause then normal exit flushes the session log.

Primary references: existing project PCSX2/R5900.cpp/IopCounters sources; ps2dev/ps2sdk `iop/sio/sio2man/include/sio2man.h` and `src/sio2man.c`. Existing nullDC4Wii-derived DSI design and attribution remain unchanged.
