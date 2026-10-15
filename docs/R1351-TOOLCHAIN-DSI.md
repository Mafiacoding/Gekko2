# R1351A / R1351B comparison

Both builds use the official devkitPro devkitppc image downloaded on 2026-10-09:
devkitPPC r50-1, powerpc-eabi GCC 16.1.0, libogc 3.1.0-1, crtls2.0.0-1,
rules1.2.1-1 and libfat-ogc2.1.0-4. The downloaded package layer SHA256 is
3727f0a7f966b9fae8b3cdbe70e976d2bd28a4c1409c525278ebd104577bc15a.
The complete image manifest is included in the comparison package. No compiler,
SDK, BIOS, game, private runtime log or user configuration is redistributed.

## Common changes

R1350 is the CPU/VU/ARM baseline. Include the real IOP thread header in main.c:
the previously implicit int declaration incorrectly described a uint64_t
diagnostic return. Pass a tmd_view pointer to the modern ES_GetTMDView API.
Both builds allocate EE RAM with 4096-byte alignment. Assembly receives the
same feature flags as C. Launcher/session logs identify A versus B and the
compiler version. The ARM worker ELF is unchanged; the malformed R1350 Wii
capability reply is not claimed fixed by this release.

R1350's uploaded Wii log24 reached FIRST_IMAGE at 412015 ms (6:52), with
295446465 EE and 44911044 IOP instructions. Its worker returned magic00000031
instead of474b4131, and267584 identical-vertex Gouraud triangles remained.
Those are baseline observations, not measurements of the new toolchain.

## R1351B DSI pilot

Adapted reference: https://github.com/BenoitAdam/nullDC4Wii,
commit26a623e3feca9927d8ffe43706b06d82d672dd64, wii/wii_fastmem.cpp.
Gekko2 retains its GPL-3.0-or-later license and identifies the adapted assembly
and HTAB design in their source files. The SH4 guest map/emitter is not reused.

Reserve a256KiB aligned HTAB using the ordinary allocator before EE RAM.
Map32MiB EE RAM, 8192 pages, read-only at PPC effective addresses
0x20000000..0x21ffffff. Use distinct SR2/SR3 VSIDs and preserve BAT mappings.
MMIO, ROM, scratchpad, guest TLB addresses, other RAM ranges and stores are
not mapped. Refuse to replace a pre-existing SDR1 page table or custom DSI
handler. Preserve and restore SDR1, SR2, SR3, the DSI handler and TLB state
on shutdown or a failed startup probe.

Startup probes read three distinct temporary sentinels through the mapped
alias, restore the original host RAM words, then prove an unmapped MMIO
address faults. Only that exact probe instruction is skippable in probe mode.
Failure disables the pilot and keeps the previous guarded memory emitter.

Only the scalar EE LW emitter changes. It retains KSEG0/KSEG1 and alignment
guards. A mapped read executes PPC lwbrx directly. An unmapped read faults at
a marked generated site. The handler validates the live arena owner, metadata,
fault opcode, forward in-owner branch, DAR window and read/page-miss DSISR.
It replaces the faulting word with a branch into the pre-existing exact
ee_mem_read32 fallback and publishes the instruction with dcbst/sync/icbi.
The assembly restores all interrupted GPRs, CR, LR, CTR and XER and executes
rfi. The memory helper runs after exception return in normal context. Later
accesses at that owner/site use the helper without repeated DSI. No stdio,
allocation, IPC, scheduler, FPU or MMIO helper executes in the handler.
The frame leaves the complete outgoing EABI parameter area free.

Descriptors are relative inline metadata inside their code owner, so final
publication/relocation does not create dangling site pointers. Released owners
are refused. Normal source/TLB/DMA invalidation remains unchanged. Stores keep
the existing generation-notification path; translated/proven RAM event windows
retain their existing path. This is not full branchless fastmem, a store DSI
implementation, a guest TLB bypass, or complete native CPU coverage.

## Test on Wii

1. Launch A. Use the same BIOS, disc, saved options and GX state as R1350.
   Cold Boot; measure time to first image. Pause and save the log normally.
2. Quit to HBC; launch B. Keep all those options identical. EE dynarec,
   6MiB code arena and Fastmem must be ON before Cold Boot. B needs no new
   IOS installation and no separate DSI option. ARM is independent of DSI.
3. In B, DSI_FASTMEM status=1 enabled=1 probes=4 indicates startup success.
   DSI_PROGRESS emitted counts translated sites; it does not count retired
   loads. faults/patched are recognized generated faults/backpatches, excluding
   startup probes. A reports compiled=0 and hardware_MMU=0.
4. If B has a negative status, record it: it is using the guarded fallback.
   Compare its output/log, but do not attribute any speed to active DSI.

Status codes:0 disabled;1 active;-1 HTAB allocation;-2 RAM alignment/size;
-3 existing table/custom handler;-4 PTE insertion;-5 alias probe;
-6 unmapped probe;-7 code arena disabled;-8 host address translation disabled.

Hardware boot, MMU/cache timing and speed improvement are not yet measured.
There is no four-minute boot or doubled FPS claim. Keep GX/ARM constant for
the A/B comparison; do not change options halfway through a run.

## Verification scope

The linked-code DSI test executes the shipped PPC initialization, allocator,
handler/glue, emitter, RAM load and normal-context fallback. It models privileged
SPR access, the Broadway hash walker and exception delivery because Unicorn
does not implement that platform. It checks8192 mappings, probe restoration,
signed RAM reads, MMIO backpatch and second-call behavior, discarded reads,
unaligned/KUSEG guards, every restored register, malformed/stale sites,
unrelated exception chaining and bounded failure/cleanup cases. It is not a
physical Wii MMU test. An instruction audit rejects FP opcodes and unexpected
calls in the fault handler and live-owner checker.

Modern libogc exports PPCDCacheFlushAsync/PPCDCacheInvalidate/PPCICacheInvalidate
under the older cache API names. The linked test loader aliases these symbols
and models libogc's lazy FP enable after PPCSync clears MSR.FP. It executes all
actual FPU instructions; it does not replace FPU arithmetic. Complete modern
FPU states are compared with the R1350 baseline.
