# R1335: IPU data, bounded HLE and optional worker transport

Wii is the primary target. No physical-Wii FPS result is asserted for this
checkpoint. The owner reports slower R1334 BIOS boot with GX/Fastmem disabled.
That is a real report; a cache regression has not yet been isolated by a
matched hardware timing run.

## What changed

- The EE lookup reads 32-byte dispatch tags before reaching full block owners.
  Source/mapping generations, pinned executable owners, budget variants and
  the 6 MiB arena remain intact.
- **EE compact cache** adds a cold-boot A/B option. With reuse ON, OFF means
  4096 owners/four ways/1024 sets; ON means 1024 owners/four ways/256 sets.
  Reuse OFF still means 256 direct-mapped owners. Allocated BSS is unchanged;
  the compact option limits the actively addressed owner/tag working set.
- IPU input bytes are retained in the eight-QWC FIFO and two internal QWCs.
  BCLR preserves the command's initial bit offset; BP/TOP are read-only.
  FDEC consumes the specified skip and peeks 32 MSB-first stream bits. SETIQ,
  SETVQ and SETTH have real table/threshold effects. Command completion raises
  INTC IPU bit 8; CTRL/DATA busy reflect the relevant operation.
- CSC converts actual RAW8 Y/Cb/Cr macroblocks to guest RGBA32 or RGBA16 using
  the documented integer conversion, threshold colour/alpha and dithering.
  Output goes through a real eight-QWC FIFO. FROMIPU writes EE RAM, not a fake
  source-to-sink transfer. TOIPU accepts only available capacity. STR/QWC/MADR
  advance with accepted bytes; normal DMA and resumable source chains retain
  pending state, including CALL/RET and TIE. Other channels keep their paths.
- EE LD/SD reach IPU registers; SQ/LQ reach the full FIFO quadword, with explicit
  guest byte order. Ordinary SQ avoids constructing FIFO bytes on the RAM path.
- Checkpoints include bounded IPU FIFO/bitstream/table/output state and pending
  DMA tags; older checkpoints default the new fields to cold state.
- Known BIOS RAM copy/fill calls have a bounded bulk path, ON by default.
  MMIO/ROM/wrapping spans keep helpers. Alias overlap retains forward-copy
  semantics; IsC stores still remain isolated. ABS/LABS avoid INT_MIN C UB.
- **Strict BIOS HLE fallback**, OFF by default, declines unknown A0/B0/C0 calls
  without changing the guest pipeline/registers. Known implemented calls keep
  HLE. The legacy zero-return approximation is counted separately. This is a
  compatibility experiment, not a complete replacement BIOS or a wholesale
  conversion of every existing SIF/module/thread workaround.
- **ARM IPU worker**, OFF by default, probes `/dev/gekko2` once, validates the
  version/capabilities, and can submit CSC through asynchronous ioctlv. IPC
  uses private aligned buffers, not guest RAM or code pointers. Epoch checks
  reject results from a prior cold boot/reset/checkpoint. Absence/failure uses
  the CPU kernel; logs distinguish enabled, available, submitted and completed.
- `TIME_SAMPLE` now exposes exclusive `IPU` time in the existing randomized
  CPU profiler. `IPU_STATUS`, `HLE_ROUTES`, `ARM_WORKER` and `EE_CACHE_LAYOUT`
  describe actual routes rather than using GX ready as a performance metric.
  The options menu pages the growing list without overlapping descriptions.

## PCSX2 versus Play! and the selected policy

PCSX2 boots a console BIOS and selectively intercepts services such as IRX
imports/host I/O. In `IopBios.cpp`, absence of a handler returns control to guest
execution; returning an interception status is not a generic guest `$v0=0`
answer. Play! supplies its own EE/IOP BIOS services and does not accept an
external BIOS. Replacing Gekko2's entire operating-system model with Play!'s
requires substantially more scheduler/module/device compatibility work.

The chosen policy is hybrid: the current real-ROM boot and guest JIT remain,
while verified, bounded services use host implementations. New unknown-call
fallback is separately testable; it is not made the boot default without a
full BIOS/game trace proving compatibility. A HLE call executes host C; guest
instructions between calls still execute the IOP/EE JIT where supported.
HLE, JIT and GX solve different workloads, not interchangeable routes.

Primary sources examined:

- PCSX2 [IPU tree](https://github.com/PCSX2/pcsx2/tree/3c8df07e8e367caef8386fd949d6ca44bf2f7ad0/pcsx2/IPU):
  IPU.cpp/IPU.h/IPU_MultiISA.cpp/yuv2rgb.cpp/IPUdither.cpp.
  The scalar conversion is adapted under GPL-3.0+ with attribution.
- PCSX2 [IopBios.cpp](https://github.com/PCSX2/pcsx2/blob/3c8df07e8e367caef8386fd949d6ca44bf2f7ad0/pcsx2/IopBios.cpp)
  and [BiosTools.cpp](https://github.com/PCSX2/pcsx2/blob/3c8df07e8e367caef8386fd949d6ca44bf2f7ad0/pcsx2/ps2/BiosTools.cpp).
- Play! [README](https://github.com/jpd002/Play-/blob/83700b2c31e593bc94e845b4b31b797be84dda59/README.md)
  and [IPU.cpp](https://github.com/jpd002/Play-/blob/83700b2c31e593bc94e845b4b31b797be84dda59/Source/ee/IPU.cpp).
  Its CSC uses a different floating-point conversion; the PCSX2 hardware-integer
  method was selected. Play!'s PACK dispatch is null at this revision, so it
  is not a universally complete IPU template either.

## Explicit remaining limitations

IDEC/BDEC/VDEC MPEG VLC/IDCT decode and PACK are still unimplemented. They retain
legacy acknowledgement and increment `unimplemented`; successful CSC is not
proof of working FMV. Timing remains functional/stream-driven, not a calibrated
IPU cycle model. Empty CPU FIFO reads return zero; direct CPU writes to a full
FIFO are counted as discarded. DMA instead stalls without discarding data.
Destination-chain FROMIPU and TOIPU MFIFO/stall-control are not provided.

`arm/worker.c` is the real portable command handler and shared CSC kernel; an
IOS resource-manager/server must register the endpoint, validate IPC vectors
and call it. No service is installed or shipped as an IOS image. The supplied
SDK lacks devkitARM, so `arm/build-worker.sh` cannot build an ARM object here.
Host worker and linked PPC client tests are not ARM hardware evidence. No ARM
clock, IPC win, IOP/VU ARM JIT or multicore gameplay claim is made.

The game archive transfer remains unavailable (the retained Tekken archives
returned HTTP 403). A Tekken title-screen boot is not validated. The retained
user BIOS is used privately for an early native boot survey, never distributed.

## Evidence and reproduction

`tools/crossbuild_r1335.sh`: two Wii DOL/ELFs. Optimized defaults to cache reuse
ON; Control defaults it OFF. Both enable the tested HLE RAM path. Existing
saved settings take priority; use a cold boot, not pause/resume, to apply them.
Option version 3 migrates older files by enabling the new HLE RAM default;
strict HLE, ARM and compact cache remain OFF unless chosen.

`tools/verify_r1335.py`: 27 linked-PPC suites, including IPU byte order, actual
64/128-bit CPU access and asynchronous worker ownership/stale epoch checks.
The transport/platform services are mocked; guest CPU code is not replaced.
Native IPU/worker tests include normal/chain backpressure, output delivery,
formats/thresholds/IRQ and malformed ABI rejection. HLE tests compare fast/helper
results, overlap/aliases and isolate-cache stores. ASan/UBSan IPU tests pass.
Package Verification.json records exact final results and artifact hashes.

The native BIOS survey runs 5,000,000 scheduler grants (39,999,852 retired EE,
4,993,049 IOP). R1334 and R1335 match EE/IOP PCs, register checksums and both RAM
checksums, with neither CPU halted. Eight old unimplemented IPU observations
become zero in this early phase. That is not a full BIOS-screen or game boot.
Host timing under concurrently running tests fluctuates; no speed ratio is
inferred. The driver is `tools/bios_survey_r1335.c` and does not patch the BIOS.

Synthetic R1334/R1335 capacity replays preserve guest results and install counts.
Tag lookup executes roughly 0.25–0.4% more PPC instructions in these fixtures;
Unicorn does not model Broadway cache misses. A warm access trace reduces reads
of full owners (1360 to 960), while adding tag reads (449), and touches 34 combined
owner/tag lines versus 29 owner lines previously. These data do **not** prove a
hardware speedup. Compact-cache A/B and matching Wii logs are required.

First owner test: GX OFF, Fastmem OFF, reuse ON, HLE RAM ON, strict HLE OFF,
ARM OFF. Compare cold boots with compact cache OFF then ON, keeping BIOS and
all other settings the same. Send the two numbered R1335 logs. Then test GX and
Fastmem independently. Measure `FIRST_IMAGE` milliseconds, compile/EE/IOP/IPU
exclusive time, cache collisions and actual routes; EE counter speed alone is
not guest-visible progress.
