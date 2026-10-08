# R1334: Starlet feasibility and future offload

Scope: researched next development area, **no ARM worker is implemented or active**.
No ARM option is exposed in this checkpoint. Android PS2 emulators demonstrate
that an ARM instruction set can host emulation; they do not establish Wii
Starlet throughput or spare service capacity.

## Hardware and available interface

Starlet is an ARM926EJ-S at 243 MHz, big endian; Broadway is a separate
729-MHz PowerPC. IOS already runs on Starlet and services the application.
The supplied SDK contains devkitPPC/libogc, not devkitARM. libogc's ipc.h
provides IOS_OpenAsync, IOS_ReadAsync, IOS_IoctlAsync and IOS_IoctlvAsync.
These send requests to an existing IOS server; they do not execute arbitrary
ARM work by themselves.

The inspected Custom IOS Module Toolkit registers /dev/haxx and shows an
IOS module plus a PPC client. Its old installer targets a particular IOS
layout (IOS31 DIP replacement / IOS254). It is a historical reference, not a
compatible generic loader we can copy into Gekko2. The inspected mload helper
uploads and starts ARM ELF modules through an already available /dev/mload
service. Gekko2 cannot assume that service exists on the owner's Wii.
Replacing IOS with mini is a different runtime with different device support,
not a drop-in parallel worker for the current libogc application.

## Candidate jobs and correctness boundary

Begin with deterministic, batched integer conversions on private snapshots,
for example a texture-format conversion. Audio decode/mixing is another
candidate only after profiling a real audio workload. Avoid early migration
of the entire PS2 IOP, EE or VU: guest memory ownership, interrupts, DMA,
SIF dependencies and exact EE8/IOP1 retirement order currently cross those
boundaries. A separate Broadway software thread alone adds no CPU core.

A first worker should negotiate protocol/version/limits before accepting jobs.
Use fixed-width integers, explicit byte order, aligned buffers, bounded lengths
and a session generation. Submission/completion is asynchronous; no busy-spin
in EE execution. Broadway flushes inputs before submission and invalidates
outputs only after completion under the selected IOS transport contract.
The worker never writes live EE/IOP RAM. Consume a result only at the correct
guest boundary, and reject old-session results after cold boot. Keep bounded
queues and enough IOS service time for storage/controllers/network.

## Measurement gate

1. Establish a compatible ARM build/service/loader arrangement in an isolated
   development setup; preserve the current application/device services.
2. Measure an empty round trip, input/output transfer and worker computation
   separately over several batch sizes. Count actual bytes copied.
3. Compare bit-identical output with the CPU implementation and include short
   workloads, queue saturation, late completion and cold-boot cancellation.
4. Compare complete frame/session time with and without offload, including
   CPU overlap, cache maintenance and waiting. Keep the CPU path if the worker
   loses or if the task does not lie on the critical path.

The decision is based on measured total cost and overlap, not MHz addition.
No percentage or FPS gain is promised before a Wii experiment.

## Sources inspected

- https://wiibrew.org/wiki/Hardware/Starlet
- https://wiibrew.org/wiki/Hardware/Broadway
- https://wiibrew.org/wiki/Custom_IOS_Module_Toolkit
- https://github.com/xerpi/Custom-IOS-Module-Toolkit
  commit c6546d9f41830876d3cc33d9d2f3fe62469321e1 (main.c, syscalls,
  linker script and PPC client inspected; no code copied).
- https://github.com/wiidev/usbloadergx/blob/enhanced/source/mload/mload.c
- https://github.com/fail0verflow/mini
- Supplied libogc: include/ogc/ipc.h, async declarations.
