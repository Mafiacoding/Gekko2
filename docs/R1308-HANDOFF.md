# Gekko2 R1308 — VU issue timing and event-free peripheral timers

This checkpoint implements selected parts of the requested VU and event scheduling phases. Neither phase is fully complete. R1307 EE/IOP precise blocks and audited scalar register residency remain in place.

## Implemented

- Per-unit Q issue state: DIV/SQRT/RSQRT use the encoded Fs/Ft lanes, finite PS2 scalar normalization, I/D status bits, 7/7/13-cycle result delays, stalls before paired upper execution, and E-bit completion flush. VU0 ResetEE clears pending issue state, accumulator and execution control.
- VU1 P/EFU: all 13 scalar operations, their 11–54-cycle latencies, WAITP completion/throughput handling, old-P visibility through MFP, E-bit flush. The transcendental polynomial reference follows PCSX2 VUops.cpp; hardware-bit-exact rounding is not claimed. These operations remain scalar, not native PPC EFU emitters.
- Lower FC/FS/FM flag operations; CLIP queries see pre-pair state and the upper CLIP wins a competing FCSET. This does not implement all FMAC flag production or delayed flag visibility.
- Upper/lower VF read hazards and same-destination discard, including lower base-update side effects. Shared admission guards make fused PPC pairs/blocks decline those cases transactionally. A zero upper destination mask cannot cause a false conflict; OPMSUB's unconditional xyz write remains special.
- I-bit literals are written after the paired upper reads old I, in scalar, fused-pair and fused-block execution.
- XGKICK packets may wrap around VU1's 16 KiB local-memory ring. The existing synchronous PATH1 parser retains EOP bounds; no asynchronous transfer queue is introduced.
- EE peripheral timer event distance: deferred intervals never cross compare or overflow transitions. The original scalar transition handles the exact event tick. MMIO, logs and save snapshots materialize current counts. Restore invalidates the cached distance. Legacy mutable-pointer access selects the conservative scalar path. Divider phase and 64-bit clock wrap are preserved.
- The hot timer tick is a leaf path without a 64-bit diagnostic update or full timer scan. SD logs add `EVENT_TIMER deferred=... boundaries=...`.

Primary references: PCSX2 `pcsx2/VUops.cpp` and `pcsx2/VU0microInterp.cpp`, inspected on 2026-10-05. PCSX2 attribution and the project's GPL-3.0 licensing remain in force.

## Validation

- Full host suite: 213/213 pass; focused final VU, I-order and ResetEE tests rerun after the final dependency guard adjustment.
- Linked Wii PPC JIT and Interpreter: 32 independent Q/P/EFU, lane selection, old-I visibility and pair-hazard oracles each. JIT adds seven native block cache, source replacement, budget, data/code alias, hazard and I-order groups.
- Raw native PPC fused-pair and block differential/ABI suites pass; declined hazard/WAITQ cases remain transactional. Updated old I-order expectations follow the primary interpreter reference. Historical nested VU runners with an obsolete IOP JAL-rejection assumption are not validation gates; the focused linked runner above is used instead.
- Existing EE/IOP regressions: 477 paired IOP programs, exact EE8/IOP1 ordering, 62 independent residency/helper oracles, and 1,152 EE merge byte-lane cases plus 64 IRQ boundaries pass.
- Peripheral timers: 30,000 randomized mixed MMIO/tick transactions compared with the original scalar implementation. IRQ counts agree at every tick, not only at the end. Tests include clock/divider wrap, wide COUNT/COMP values, zero return and retained mutable state.

## Measured cost, not hardware FPS

Actual linked PPC instructions in 4,096 isolated timer ticks, one active timer:

| Clock | R1307 | R1308 | Reduction |
| --- | ---: | ---: | ---: |
| BUSCLK | 196608 | 45423 | 76.9% |
| BUSCLK/16 | 165888 | 45423 | 72.6% |
| BUSCLK/256 | 184448 | 45428 | 75.4% |
| HBLNK | 217088 | 45428 | 79.1% |

Complete warm eight-retirement synthetic CPU samples:

| Program | R1307 | R1308 |
| --- | ---: | ---: |
| EE ADDIU self | 3244 | 3052 |
| EE OR sources | 3292 | 3100 |
| EE XOR accumulator | 3302 | 3110 |
| EE LW base | 3893 | 3701 |
| IOP ADDIU self | 6590 | 6590 |
| IOP OR sources | 6637 | 6637 |
| IOP XOR accumulator | 6642 | 6642 |
| IOP LW base | 8335 | 8335 |

These fixtures mock host services and count instructions, not Wii cycles. They do not establish BIOS FPS, input responsiveness or real hardware boot time. New VU correctness bookkeeping can add cost; no universal VU speedup is claimed.

## Still open

Full FMAC/IALU scoreboard hazards and delayed MAC/status/CLIP flags; D/T/M interrupt semantics; pipeline-accurate branch corner cases; asynchronous VU/VIF/GIF/XGKICK arbitration; native EFU emitters; general CPU Count/Compare/SIF/GS event batching. CPU retirement and the EE8/IOP1 interleave are unchanged. Full dynarec coverage/allocation/direct linking also remain outside this checkpoint.

## Wii test

Copy the JIT DOL to `sd:/apps/gekko2/boot.dol`, using the existing BIOS/SD layout. Coldboot with the same BIOS and GX setting as R1307, compare Sony-to-OSDSYS time and FPS, and return `sd:/pcsx2/Gekko2-R1308-gx-render.log` (or `...-software.log`). The timer line helps distinguish batched intervals from real boundary work. Reset to a fresh coldboot; raw emulation save states from an older structure layout are not interchangeable with R1308. This source checkpoint archive is separate from an emulation save state.
