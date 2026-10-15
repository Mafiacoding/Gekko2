# R1342 — bounded EE reuse, CPU service guards and IOP deadlines

## Outcome and scope

This build targets CPU execution and scheduler overhead identified in the owner's
R1341 BIOS log. It does not establish doubled FPS, a halved BIOS boot, full
Tekken compatibility, or physical ARM startup. The historical R1337 log filenames
remain; the BUILD row identifies R1342.

The supplied BIOS profile attributed approximately 54% to EE execution, 23% to
scheduler and 13% to IOP, versus 2.2% to GS. These are instrumented software
categories, not percentages of Wii CPU/RAM/GPU utilization. FIRST_IMAGE was
515345 ms in R1341 versus 507757 ms in R1339. At that workload, CPU work is the
first target; transferring more primitives to GX alone cannot explain a 2x gain.

## Changes

1. **EE cached block prefixes.** A warmed, validated longer owner can run an
   exact shorter grant through a lazily generated bounded PPC body. The owner
   retains its ordinary full body and source identity. It no longer needs a
   separate short owner merely to satisfy that grant. Each instruction still
   receives source/TLB validation and precise preparation/retirement. Branches,
   delay slots, interrupts and memory callbacks keep their boundaries. Prefix
   bodies do not chain successors. Both bodies are released with their owner;
   existing executing-code pinning remains. Allocation failure retains fallback.
   This costs additional code and one-time compilation for owners that need a
   prefix; it is not a free cache-capacity increase.

2. **CPU inactive service guards.** Avoid calls to EE helpers when their pending
   state or exact PC cannot match; VBlank/GS checks run at the existing display
   phase transitions. Five exact-address IOP HLE gate families are skipped above
   their maximum gate address (0x44c). The IOP module loader's live-code checks
   remain outside that guard. Count/Compare, interrupt latching, timer updates,
   SIF ordering and the original 8:1 EE/IOP schedule remain. Parked EE ticks use the same guards;
   mandatory clock and interrupt work remains. Scheduler callbacks reuse core
   state pointers refreshed at each frontend invocation instead of getter calls.

3. **IOP alarm / delay deadlines.** Cache the minimum active alarm or delayed
   thread deadline rather than scanning both fixed tables every retirement.
   Every recognized THREADMAN call invalidates the derived hint. When due, run
   the unchanged scan: lowest due alarm slot first, one alarm dispatch at a time,
   then delayed wakes. Preserve the existing instructions_executed clock; this
   is not a new cycle-accurate timing model. Exposing the mutable checkpoint blob
   disables hints until reset, because a retained pointer can change later.
   Hint statistics are outside the checkpoint, whose layout stays unchanged.

4. **Light runtime diagnostics.** Skip historical per-PC observational probes
   and the old sampled retirement detail timer, plus the historical
   R1214/R1217/R1218/R1230/R1234/R1235 fetched-instruction probes. Current session, FIRST_IMAGE,
   fault, performance/time, cache and ARM records remain. Some older counters
   outside that diagnostic region still run. No FAST build mode that alters
   guest behavior is enabled.

5. **MLOAD transfer correction.** The observed READ status 1 is not treated as a
   one-byte transfer. Accept nonnegative transport statuses, then prove actual
   bytes. Preflight every segment with a poisoned read buffer before any write;
   seek each chunk absolutely; read back each write against a complement-poisoned
   buffer and compare every byte. Missing/partial bytes and corrupt writes still
   fail; no thread starts after failed verification. ELF/base/capacity/stack
   validation remains. The ARM worker binary is unchanged and CSC-only. BIOS
   traces with zero CSC jobs cannot gain from enabling that worker.

6. **Experimental ARM BIOS RAM jobs.** A separate default-OFF option uses the
   existing ARM-side MLOAD memory service for large ordinary-RAM copy/fill calls
   already recognized by the IOP A0 BIOS HLE helpers. Destination/source spans
   must be 32-byte aligned, 64 KiB–1 MiB, entirely in shared MEM1/MEM2 and
   nonoverlapping for copy. Guest RAM bounds, Status.IsC, forward-copy overlap
   semantics and MMIO/ROM fallbacks stay in the original helpers. Flush source
   and destination caches before transfer; invalidate destination after each
   completed IOS request; verify every returned byte. Negative status, missing
   MLOAD, short/missing/corrupt data or ineligible spans use the ordinary CPU
   path. A partial failed copy is safe to repeat because source/destination do
   not overlap. Synchronous requests finish before fallback, so no late ARM
   writer races the CPU. Logs distinguish chunks from complete verified jobs.
   This path needs MLOAD but not a functioning CSC worker or a new ARM ELF.
   It does not execute EE/IOP instructions on ARM, convert arbitrary BIOS
   loops into jobs, or run the PPC JIT cache there. Full byte verification and
   synchronous IPC cost CPU time; it can be slower and is not a speed claim.

Existing Fastmem, GX, IPU MPEG/IDCT, audio streaming and controller code remains
under its existing contracts. These changes do not complete FMV/audio playback.

## Options and migration

Four independent options default ON: EE cached block prefixes, CPU inactive
service guards, IOP alarm / delay deadlines, Light runtime diagnostics.
Old versions 1–4 migrate to config version 5 with those four defaults enabled;
old option bits including Fastmem, GX, Strict, ARM and IOS selection retain their
values. A version-5 explicit OFF persists. ARM BIOS RAM jobs defaults OFF and has its
 own persistent option; its IOS selection applies at app restart. The native host hides unsupported PPC
options. Changes apply on **COLD BOOT / NEW LOG**, not pause/resume.
ARM/IOS changes require saving, fully exiting to HBC and restarting the app.

Suggested first test: retain the last comparable R1341 settings, leave EE/IOP
JIT and block-cache reuse ON, EE Compact and Strict OFF, and use the four new
options ON. Do not change GX/Fastmem simultaneously if measuring the new build.
For a control, disable only these four and cold boot again. Compare FIRST_IMAGE
elapsed ms at the same BIOS phase; total EE instruction counts are not FPS.
For Tekken, PLUS pauses and MINUS+A sends PS2 START; no Nunchuk is required.

New log rows: EE_PREFIX runs/retired/enabled/services and IOP_DEADLINES
skipped/scans/exposed/enabled. BUILD masks identify Light diagnostics too.
ARM_RAM completed/bytes proves verified memory jobs; available=1 only proves
MLOAD opened. To test RAM jobs: select the experimental option ON, retain/select
IOS222 if that is your installed MLOAD IOS, save, fully exit to HBC, restart and
cold boot. Compare the same BIOS phase with RAM jobs OFF. No jobs means this
BIOS path did not use eligible A0 helpers; extra IPC may regress boot time.
ARM_WORKER available=1 proves the CSC handshake; submitted/completed proves work.
Successful transport tests do not prove a physical Wii handshake.

## Measurements

Actual linked Wii ELF code executed in Unicorn PPC; allocator/cache-maintenance
and platform services are mocked. Counts are host PPC instructions, **not Wii
time or FPS**. Equal guest state is a required condition.

| Workload | Control | R1342 / enabled | Interpretation |
| --- | ---: | ---: | --- |
| Repeated short EE grants, warmed prefix option OFF/ON | 1479400 | 1165200 | 21.24% fewer instructions; compilation excluded |
| Short-grant collision replay, R1341/R1342 | 461755 | 413555 | 10.44% fewer; 1034 exact guest retirements |
| Mixed EE/IOP/SIF, R1341/R1342 | 29850150 | 29650001 | 0.67% fewer; 4096 EE / 512 IOP measured slots |
| THREADMAN no-event scan, OFF/ON, 1000 ticks | 406000 | 37000 | 90.89% fewer in this helper, not whole emulator |
| Prefix differential suite including cold compilation, OFF/ON | 330013 | 641771 | One-time bounded-body generation is more expensive |

Mixed replay guest signature:
b2a0fade1350ec4bcd45e2ff6c3b0cb2fb273dd2874022520f1c71814c2737a8.
There is no basis to convert these selected synthetic gains into a claimed 2x
BIOS speedup. Actual R1342 Wii/Dolphin boot and ARM startup need owner testing.

## Verification and reproduction

Run tools/verify_r1338.py against the built ELF: 35 focused linked-PPC groups.
Additional linked tests: verify_prefix_r1342.py (34 exact grants plus warm reuse,
source mutation and teardown), verify_iop_deadlines_r1342.py, controller latch,
IOP fetch, RSPU2 streaming, endian/page/source bounds and ARM RAM jobs.
The latter executes actual PPC clients and BIOS helpers with mocked IOS: status
1 complete, unavailable MLOAD, seek error, partial/missing data and transfer
error; success/fallback produce exact memory and BIOS return state. Copy/fill
also rewrites a warmed IOP owner and proves that new instructions execute.
Prefix tests compare complete EE state and touched
RAM, including branch/delay boundaries and loads/stores.

Native test_iop_deadlines_r1342 compares 12000 cumulative complete thread/CPU
traces with real alarm cancellation, delay, release, suspension, clock wrap and
mutable-checkpoint fallback. Native option tests cover old config migration and
version-5 explicit OFF roundtrip. ARM loader tests cover 14 status/data scenarios,
including status 1 with full data and with incomplete reads/writes. ARM and
THREADMAN tests run with ASan/UBSan; leak sanitizer is disabled in this runtime.
DOL sections must match ELF bytes and the Wii startup signatures.

Build with devkitPPC r32 / GCC8 and libogc 1.8.18:
```sh
make -j3 BUILD=build-r1342 TARGET=Gekko2-R1342 \
  EXTRA_CFLAGS='-DGEKKO2_EE_BLOCK_FAST -DGEKKO2_GOURAUD_EARLY_DEGENERATE -DGEKKO2_ARM_BETA'
```
Set DEVKITPRO/DEVKITPPC to the installed SDK. Host verification needs Unicorn
2.1.4 and the SDK nm/compiler tools. No BIOS, disc, private RAM/logs or SDK is
included in the source checkpoint.

## Primary-source audit and decisions

Examined source, not only project summaries, on 2026-10-08 UTC:

- [PCSX2 R5900.cpp](https://github.com/PCSX2/pcsx2/blob/aa7ab4306e269075784c7ac3eb4b45e6e6c53445/pcsx2/R5900.cpp),
  [Counters.cpp](https://github.com/PCSX2/pcsx2/blob/aa7ab4306e269075784c7ac3eb4b45e6e6c53445/pcsx2/Counters.cpp),
  [iR5900.cpp](https://github.com/PCSX2/pcsx2/blob/aa7ab4306e269075784c7ac3eb4b45e6e6c53445/pcsx2/x86/ix86-32/iR5900.cpp):
  next-event scheduling and grouped rare-device checks motivate avoiding inactive
  service work. Their cycle model, x86 register allocator and interrupt delays
  cannot be transplanted into this instruction-clock PPC core unchanged.

- [Play! BasicBlock.cpp](https://github.com/jpd002/Play-/blob/83700b2c31e593bc94e845b4b31b797be84dda59/Source/BasicBlock.cpp):
  quota/accounting and block-link lifetime support examining grant overhead.
  Gekko2 retains precise per-instruction checks; it does not use an unchecked
  instruction subtraction or import Play!'s JIT framework.

- nullDC4Wii revision 26a623e3feca9927d8ffe43706b06d82d672dd64:
  [SH4 scheduler](https://github.com/BenoitAdam/nullDC4Wii/blob/26a623e3feca9927d8ffe43706b06d82d672dd64/dc/sh4/sh4_sched.cpp),
  [block manager](https://github.com/BenoitAdam/nullDC4Wii/blob/26a623e3feca9927d8ffe43706b06d82d672dd64/dc/sh4/rec_v2/blockmanager.cpp),
  [PPC driver](https://github.com/BenoitAdam/nullDC4Wii/blob/26a623e3feca9927d8ffe43706b06d82d672dd64/wii/dc/sh4/rec_v2/wii_driver.cpp),
  [GX renderer](https://github.com/BenoitAdam/nullDC4Wii/blob/26a623e3feca9927d8ffe43706b06d82d672dd64/plugs/drkPvr/gxRend.cpp).
  Deadline and block-owner reuse patterns are relevant. Gekko2 already has a
  6-MiB arena, preserved PPC GPR residency and supported GX depth/blend/texture
  reuse; larger RAM alone does not remove retirement work. SH4 FPU pinning and
  fault-patched Fastmem are not safe substitutes for PS2 TLB/MMIO semantics.
  Dreamcast polygon strips are not equivalent to every PS2 GS state. More GX
  coverage remains useful for GS-heavy workloads, but is not the first measured
  BIOS bottleneck. No unsupported blend/Z/alpha state is silently moved to GX.

- [Hermes MLOAD PPC wrapper](https://github.com/wiidev/usbloadergx/blob/e25c4f3501ed957b7db73f79c51fdf00715ab2e2/source/mload/mload.c)
  and [mload-mod server](https://github.com/xerpi/mload-mod/blob/main/source/main.c):
  IOS wrapper return status is not a portable byte-count contract. The latter
  server is a reference implementation, not identified as the owner's IOS222.
  Thus data poisoning/readback proves completeness independently of status.

An aligned memcpy/bswap source-read experiment was rejected: GCC already
emits Broadway lwbrx for the existing byte-source expression. The targeted
short-grant instruction counts did not improve; existing source code stays.

No EE machine-code cache is offloaded to ARM. ARM cannot execute cached PPC code;
shipping state/coherence work across IOS IPC is not an evidenced replacement
for reducing EE retirement overhead. The added MLOAD path is bounded RAM
assistance for already recognized BIOS helpers, with explicit CPU verification
cost. Full decoder offload needs a separate
protocol and timing study. No new graphics API is required for these fixes.

