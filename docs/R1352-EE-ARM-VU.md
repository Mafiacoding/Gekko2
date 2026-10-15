# R1352 Beta — EE windows, budget exits, ARM word transport, VU mapping

## Changes from R1351C

- Native event windows accept dependent RAM addresses from nonfaulting low-address arithmetic and proven RAM loads. A pure address preview runs before guest publication. Unknown operations, missing/invalid guest mappings, MMIO, alignment errors, changed code and imminent timer/IRQ/VBlank deadlines still decline. No RAM stores or device reads occur inside a window. The emitter supplies an immutable base-register mask to avoid rebuilding it on each entry. `EE_NATIVE_WINDOW dependent_runs` counts successful dependent windows.
- Bounded EE prefixes fall through their common budget checks; retirement/return stubs are placed after the body. Existing short-grant prefix reuse, cache ownership, source epochs, DMA invalidation and 8:1 scheduling remain exact. `budget_misses` includes successful prefix use: it is not an interpreter instruction counter.
- DSI supports precise `LW` and zero-extending `LWU`, including fetched-first/block-entry access. KSEG addresses can reach the normal-context MMIO fallback after a host fault; guest-TLB addresses must prove real RAM first. Event windows keep nonfaulting RAM proofs. Existing exception glue, unknown-fault chaining, HTAB self-tests and owner checks remain intact. Broader coverage is not a claim of measured Wii speed.
- ARM IPC now copies shared buffers exclusively through aligned volatile 32-bit transfers. Capabilities, CSC headers/input/output and device-name checks use private MEM2 staging for byte-oriented codec/string operations. Strict capability validation remains intact; corrupted magic is never accepted. The new ARM ELF is built with devkitARM GCC15.2.0. Prior code emitted STRB into shared buffers; d2x-cios libc also handles MEM1 with word accesses. The physical-Wii handshake remains to be verified.
- EE local memory dispatch now maps VU0 micro/data (4KiB, mirrored within each16KiB window) and VU1 micro/data (16KiB each), including KSEG aliases and TLB-translated aliases. Existing VU native code checks live instruction words. `VU_EE_WINDOWS` reports accesses.
- CPU quadword writes to VIF0/VIF1/GIF FIFOs reach the existing parsers instead of silently falling through unbacked memory. IPU handling remains. `EE_CPU_FIFO` reports deliveries.

## Important geometry correction

The log's `GS_VERTEX_SOURCE path=1` is a ZERO-BASED index: 0=real PATH1 (VU1 XGKICK), 1=real PATH2 (VIF DIRECT), 2=real PATH3 (GIF DMA). The R1351C null vertices therefore do not prove a VU1 XGKICK defect. The VU mappings and CPU FIFO gaps are independently established fixes. Their effect on the BIOS's237216 degenerate Gouraud triangles is still unverified. Do not fabricate coordinates or force degenerate triangles into GX.

## Validation and measurements

Actual linked PPC execution under Unicorn:154 complete EE/timer/IRQ/display-state comparisons, grants2..8, including pointer increments, LUI/ORI pointers, loaded pointers, LWU,64-bit add low-address dependencies and replacement of an unknown preview value. Seven pure declines preserve guest state. Six dependent-address benchmarks use18–22% fewer executed PPC instructions than the precise path (100 eight-instruction blocks per case). This is an instruction-count result, NOT Wii time or FPS. The simple window benchmark uses154142 versus268942 PPC instructions for precise execution; R1351C's existing simple native window used160642.

DSI extended suite:28 full-state pairs across short grants, rs=rt, branch-delay MMIO, unaligned and missing TLB, LWU and MMIO-first blocks, plus48 scalar signed RAM cases, patch-once/fallback/owner rejection and platform-state restoration. Privileged MMU/cache/exception delivery is modeled; actual emitted PPC bodies and exception glue execute.

ARM shipped ELF: strict word-only shared-buffer hooks, exact32-byte capability reply, CSC RGB32 for1/3/8 blocks, RGB16/dither, queue/open/caps/ioctlv, rejection of invalid lengths/counts/spans/overlaps. IOS syscalls/cache behavior are modeled. New shipped ELF also passes the actual host-side ELF load-plan validator.

Native tests cover VU windows and mirroring, CPU SQ FIFO/fragmented DIRECT handling. Existing VIF suite,192 VU lane-mask cases plus4 dual-issue hazards, precise prefixes/arena pressure,40 branch/delay-slot budget-edge interpreter comparisons,11 EE/IOP/SIF grants and30+68 helper-EABI cases remain regression gates. See verification logs in the release package.

## Wii installation/test

1. Install apps/Gekko2-R1352/boot.dol (or replace your launcher DOL).
2. Replace sd:/pcsx2/arm/Gekko2-ARM-Worker.elf with this package's NEW file. Merely replacing the DOL leaves the old worker on SD.
3. Fully restart the Wii before this ARM test: an already registered old IOS resource manager can survive a DOL restart. Keep the same saved settings/IOS222 as your C comparison and then Cold Boot the emulator.
4. Let BIOS reach first image, pause and exit normally to save the log.
5. Check BUILD R1352, ARM_WORKER_CAPS decline=0/magic474b4131 and available=1, DSI block_prepares, EE_NATIVE_WINDOW dependent_runs, VU_EE_WINDOWS, EE_CPU_FIFO, FIRST_IMAGE and GS_GOURAUD visible/degenerate.

ARM CSC may legitimately submit no work during BIOS boot (the prior BIOS log has no CSC commands); worker availability is not evidence of BIOS CPU offload. DSI does not require ARM or IOS222. A four-minute boot, doubled FPS and repaired BIOS geometry are not established by these tests.

## Primary implementation references

- Local PCSX2 Memory.cpp memMapVUmicro (0x11000000/4000/8000/c000), HwWrite.cpp _hwWrite128 and official FiFo.cpp WriteFIFO_VIF0/VIF1/GIF.
- https://github.com/wiidev/d2x-cios/blob/master/source/cios-lib/libc.c (MEM1 word-access implementation), IPC/syscall contracts from the same project.
- DSI inherited adaptation: nullDC4Wii commit26a623e3feca9927d8ffe43706b06d82d672dd64, wii/wii_fastmem.cpp (GPL3+ attribution retained).

No BIOS, game data or SDK binaries are redistributed.
