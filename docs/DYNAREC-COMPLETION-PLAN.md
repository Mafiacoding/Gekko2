# Gekko2 full dynarec completion plan — development after R1306

A complete dynarec is a set of independently verified execution paths, not a single switch. This is the implementation order and definition of readiness. A path is not marked complete merely because an opcode emitter exists. Released-build status is in ../STATUS.md; unreleased source progress and verification are in DYNAREC-WORKING-NOTES.md.

| Step | Current state | Required implementation | Readiness gate |
| --- | --- | --- | --- |
| Precise EE RAM blocks | Integrated for byte/half/word, LD/SD, LQ/SQ and LWC1/SWC1 | Remaining merge/unaligned and COP2 memory paths with live proofs/fallback | Full register/memory/COP0 parity, aliases, bounds, alignment, MMIO and source mutation |
| EE control blocks | Selected branches/jumps may terminate a block; delay slot executes through scalar frontend | Fuse legal delay slots, remaining link/REGIMM/COP branches, annulment and nested/exception boundaries | Taken/not-taken/likely, budgets, link overlap, EPC/BD, every IRQ position and mutated delay-slot source |
| Register residency/allocation | General EE/IOP word binding/spill pass integrated; cross-instruction/helper residency remains open | Audit every preparation/retirement/helper write contract; liveness/dirty masks, paired 64-bit and 128-bit state, spill/flush/reload rules | Every exit exposes correct architectural state; helpers/IRQs/context switches cannot observe stale values; preserve PPC EABI |
| Safe block links/cache | Bounded caches and live per-instruction source/mapping checks | Translation/source generations, DMA/SW code invalidation, memory pressure/eviction and legal link patching | Changed TLB/ASID/code never executes a stale path; executing memory never freed; cold/warm/collision/alloc-failure parity |
| IOP blocks | Bounded cached native blocks integrated with the exact existing EE8/IOP1 scheduler; baseline parity tests pass | Delayed loads/merge forwarding and EPC/BD/TAR/alignment/overflow precision implemented with independent oracles; continue HLE/context audit, fast boundaries and hardware profiling | Block/scalar state, exact interleave and independent pipeline oracles are tested; broader hardware accuracy and repeated Wii boots remain open |
| EE instruction coverage | Many scalar families; conservative block policy | Audit and finish MMI/COP0/COP1/COP2, traps, saturations/flags, special register/exception cases | Actual emitted PPC differential tests per instruction family; unsupported cases decline correctly |
| VU0/VU1 coverage | Guarded native pairs and short blocks | Remaining flags, Q/P/EFU, upper/lower hazards, pipeline delays, branches/E-bit, XGKICK and VIF/GIF interactions | Bit/state parity over microprograms plus complete DMA/GIF results and boundary events |
| Event scheduling/performance | Per-instruction retirement checks retained | Profile real hardware, batch only proven event-free intervals; precise Count/Compare and wake-up/idle exits | Scheduler/interrupt traces unchanged at boundaries; repeated Wii coldboot and OSDSYS navigation |

## R1306 readiness

This revision extends the first two rows. It does **not** complete general allocation, block linking, IOP blocks or VU emulation. Unsupported paths retain their existing interpreter/scalar behavior. The branch-ending block is deliberately an intermediate stage before legal delay-slot fusion.

The six new memory families preserve 64/128-bit lanes, raw FPR bits and fpr[0]. LQ/SQ round effective addresses down to 16-byte alignment; LQ with destination zero performs no memory access. Loads and stores have explicit opcode classifications so LD/LWC1 are not accidentally treated as writes and SQ is not treated as a load.

## Helper contract audit before allocation

Preparation reads source/mapping and resets transient state/r0; retirement updates Count/devices/interrupts and may expose or alter execution context. A pinned register plan from nullDC4Wii cannot assume these helpers leave Gekko2 guest GPRs unchanged. For every helper, enumerate reads/writes, possible callbacks/context switches and exceptions, then choose flush/reload or a proven no-write fast path. Never infer safety from a C function's name or PPC callee-save behavior alone.

## Performance acceptance

Maintain paired Interpreter/JIT builds and an old-JIT baseline. Actual linked PPC tests use mocked platform services and measure instruction counts, not elapsed hardware cycles. Report slower workloads as well as faster ones. Whole-BIOS or game FPS claims require repeated owner/hardware runs under identical BIOS, image/controller and GX/software settings. Dolphin timing is separate from a real Wii.

## Reference projects

The pinned nullDC4Wii code review is in NULLDC4WII-REVIEW-R1304.md. Its register/helper contracts, block lookup and source-guard design inform the audit. No foreign engine or code has been transplanted; Wii64 and Lightrec are not integrated cores. GX processes supported graphics work; EE/IOP I/O, MIPS execution and scheduling remain CPU responsibilities.
