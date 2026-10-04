# R1278: aligned EE 64-bit reads and cost-aware LD execution

Mapped eight-byte reads in ee_mem_read64 now use two explicit little-endian 32-bit halves when the host pointer is four-byte aligned. GCC emits native byte-reversed PPC word loads for these halves. Unaligned pointers retain the previous eight-byte assembly loop; caller bounds, TLB fault handling and GS MMIO routing remain unchanged.

The CPU loop executes LD directly through its improved C body rather than the more expensive single-instruction native dispatch. The native LD translator/API remains available for future blocks and direct tests. The LD exclusion is after existing cheap-scalar checks, preserving the R1277 scalar measurements.

A manual PPC fast path for 32-bit reads was rejected after measurements showed extra overhead: GCC already emitted efficient lwbrx. Final EE/IOP 32-bit source behavior and measured costs match R1277.

## Actual Wii ELF measurements

All values are PPC instruction counts, with platform allocation/cache maintenance mocked. They are not cycles, a BIOS workload profile or physical Wii/Dolphin FPS.

| Workload | R1277 | R1278 | Change |
|---|---:|---:|---:|
| Aligned EE 64-bit helper read | 49 | 26 | 46.94% fewer |
| Unaligned EE 64-bit helper read | 49 | 57 | 16.33% more (alignment guard/fallback) |
| Eight LD, JIT build cold | 5345 | 4361 | 18.41% fewer |
| Eight LD, JIT build warm | 4761 | 4361 | 8.40% fewer |
| Eight LD, Interpreter build | 4649 | 4361 | 6.19% fewer |

LD samples check register value and PC progression with full retirement; no guest instructions/events are removed. Aligned addresses here include host-pointer alignment modulo eight of zero and four. The unaligned cost is retained explicitly rather than claiming every read is faster. Prior scalar workloads are unchanged (e.g. ADDIU 4049, LUI/ADDU 4161 PPC instructions per eight operations).

## Validation

182 native regressions pass. Both final ELF builds pass inherited EE/IOP/cache/IRQ/display-clock/GS/config/VU0/VU1/VIF/frontend checks. Additional PPC checks compare 512 32-bit and 256 64-bit reads against an independent byte oracle across eight alignments, including signed/high-bit patterns. A genuine CPU LD from the KSEG1 GS CSR mirror checks MMIO routing and retirement. Both halves of the EABI uint64_t return are checked.

Native authentic menu checkpoint continuation advances EE 2,048,231,103 -> 2,051,427,132, no EE/IOP halt. This is warm interpreter continuation, not a fresh PPC cold boot or hardware timing proof. Source checkpoint and separate private native recovery state are saved. Public package contains no BIOS/game/guest RAM.

The SCEA-specific slowdown remains unlocalized without the hardware PERF log (sd:/pcsx2/R1278-boot.log). No 0.60 FPS claim is made. Full EE block execution, GS mixed texture filtering and VU pipeline/float limitations remain open. R1276 SD image selection and antialiased launcher, and R1277 independent display-phase optimization remain included.

Build tools/build_r1278.sh with devkitPPC/libogc, separate engine directories, -j1. A zero-byte stale EE object was removed and rebuilt before successful linking. DOL/ELF, exact cumulative patch/diff and verification logs are included.
