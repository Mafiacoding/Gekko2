# R1341: guarded cache hints, retirement overhead and MLOAD transport

Physical Wii is the primary target. The owner's R1340 hour-long Tekken log
records 2,527,090,643 EE retirements, 288 video / 21 audio sectors, zero
RSPU transport failures, and reports reaching the Insert Coins graphic.
Interactive input was not confirmed. Exclusive CPU samples are roughly
47.3% EE, 27.6% scheduler, 13.3% IOP, 7.7% GS raster and 4.0% compilation.
Core execution occupies 98.55% of logged PERF wall intervals. These are
software timings, not hardware CPU/GPU utilization or RAM bandwidth counters.
The six-MiB code arena used 2,349,056 bytes; exhaustion is not shown.

## Implemented CPU work

- Exact best-way hints for repeated EE PC/budget/layout lookups, plus negative
  admission hints for already-declined short budgets. Both are cleared on set
  publication/release/reset and guarded across compact/control layouts.
  Hints select metadata only; source/mapping generations and every emitted
  instruction boundary are still validated. No executable ownership moves.
- EE retirement reuses the immediately preceding INTC line latch instead of
  sampling it twice. General callers (ERET and parked execution) still latch
  live state. Timer/IRQ/delay-slot and Count/Compare rules remain.
- EE/IOP deferred timer increments use the existing event-safe common path
  inline on PPC. Boundary events, MMIO/snapshot materialization and exposed
  mutable state still take the original timer logic. Timer clock is not sped
  up: the host bookkeeping is changed, not guest-visible timing.
- IOP warm lookup does not fetch its first instruction twice. Low-RAM kernel
  preparation reuses its already-fetched instruction for the zero-code check
  and execution. Remaining source words and native prepare guards stay live.
- Scheduler EE:IOP order, frontend limits and host group size remain as R1340.
  Same-PC blocks of different lengths are intentional budget variants. The
  synthetic cache test rejects identical PC/count/encoding duplicate owners;
  virtual aliases with different exception PCs are not interchangeable.

EE_CACHE_BUDGET adds refusal_hits and lookup_hits. No new checkbox is needed;
cache hints follow existing Cache Reuse. Strict and EE Compact need not be
turned on. Compare against R1340 with exactly matching other options.

## ARM transport

The owner's log authorizes 0x13700000 / 0x80000 and validates the worker but
returns ARM_LOADER=-7 before writing or starting it. Existing code required
IOS_Read/Write to return the byte count. The Hermes PPC MLOAD client accepts
nonnegative writes; support status-zero and exact-length returns here, rejecting
positive short transfers. Each chunk uses an absolute seek, poisoned reads,
all-target-zero preflight and verified write readback. Missing/partial data
cannot silently authorize an occupied target. ELF bounds/stack/metadata and
unused-target requirements remain. No IOS/NAND installer or patch is added.

ARM_TRANSFER logs operation (1 seek, 2 preflight read, 3 write, 4 readback),
signed result, absolute address and requested length. -11 denotes failed write
readback. The ARM ELF is unchanged from R1339/R1340. Actual Wii startup still
needs ARM_WORKER available=1 and real submitted/completed jobs. The worker is
CSC-only: the supplied BIOS and Tekken logs contain no CSC requests, so this
worker cannot account for their current speed.

Reference: https://github.com/wiidev/usbloadergx/blob/enhanced/source/mload/mload.c
Retain Hermes GPL attribution and the project's GPL notices.

## Input

Frontend presses are retained until an actual SIO2 read or HLE pad-buffer
sample consumes them. This prevents a short real press vanishing between very
slow guest frames; it does not inject progress or auto-press a button. Held
buttons stay held and release after consumption. PAD_INPUT records physical
held mask, guest pressed mask, sample/serial counts and remote error.
PLUS/START still pauses; Wii MINUS+A sends PS2 START. Nunchuk is not needed
for that combination. HOME pauses/returns to launcher as before.

## Verification and performance limits

35 final linked-PPC regression groups pass, including native EE/IOP, precise
IRQ/timers, delay/load slots, TLB/mutation/eviction, cache, Fastmem, VU, GX,
IPU decoder and IOS-option paths. Additional actual linked-PPC input/SIO2,
low-RAM IOP fetch and R1340 RSPU streaming checks pass. DOL bytes match ELF.
Platform services and synthetic sector I/O are mocked; no physical R1341 Wii
or Dolphin execution is claimed.

ASan/UBSan: ten ARM transfer scenarios (including status-zero success, missing
read, corrupt readback, short I/O, invalid area, occupied target and failed
seek), short-press lifecycle, and 30,000 each EE/IOP timer differential
transactions with exact per-tick IRQ traces, MMIO, overflow and snapshots.

Synthetic linked-PPC comparison with R1340:

| Workload | R1340 PPC instructions | R1341 PPC instructions |
| --- | ---: | ---: |
| Repeated short EE grants, same guest results | 491600 | 461755 |
| Warm mixed EE/IOP/SIF, 4096 EE slots / 512 IOP ticks | 29819796 | 29850150 |

The targeted workload uses 6.07% fewer instructions. The mixed workload is
0.10% higher, with identical final signatures: this is not a universal speedup.
Neither result establishes a doubled/tripled emulator or halves BIOS boot.
Further performance work needs physical FIRST_IMAGE, PERF and category samples
at matching phases. Do not claim 2x/3x or hardware ARM acceleration from this
checkpoint. Native CPU tests do not execute the Wii PPC JIT.
