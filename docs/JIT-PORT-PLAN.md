# Gekko2 PPC dynarec plan — R1303

The custom EE/IOP/VU PowerPC backend is implemented in part; it is not a complete recompiler. Read [STATUS](../STATUS.md) and [R1301 handoff](R1301-HANDOFF.md) for verified scope.

## Current execution

EE and IOP have native scalar paths with interpreter fallback. VU has guarded arithmetic pairs and bounded straight-line blocks. The precise EE executor admits short nontrapping ALU/COP1 sequences, retaining prepare/source validation and retirement/interrupt checks per instruction. R1301 uses direct relative PPC helper calls when aligned and reachable, with the absolute fallback retained.

The older R1282 resident ALU transformation primitive remains separate from the CPU retirement engine. A general register allocator, broad memory/control blocks, linked EE/IOP execution and complete VU pipeline coverage are unfinished. EE/IOP guest execution and device scheduling remain CPU responsibilities; GX is for compatible graphics operations.

## Next implementation steps

1. Profile actual Wii costs under identical BIOS, engine and rendering settings.
2. Add liveness/dirty-value allocation, preserving 64-bit halves, 128-bit lanes and the PPC EABI.
3. Extend guarded memory/control blocks with alignment, TLB, MMIO, exception and branch-delay exits.
4. Add IOP blocks while preserving load/branch hazards, interrupts and interleave.
5. Link blocks only with valid translation/source generations and safe invalidation; never free executing code.
6. Complete remaining MMI/COP0/COP1/COP2 and VU pipeline/flag/Q/P/EFU cases using verified fallback where necessary.
7. Improve event scheduling/idle handling only with precise device/timer/Count/Compare evidence.

Wii64 and nullDC4Wii may inform design. No imported core or Lightrec integration is present. PCSX2 x86 code cannot serve as a drop-in Wii PPC backend.

## Acceptance checks

Execute actual emitted/linked PPC and compare interpreter state. Include every interrupt position, budgets, delay slots/annulment, exceptions/EPC/BD, self-modifying code, DMA aliases, TLB replacement, far-call fallback and preserved registers. Cold-boot BIOS tests and repeated Wii navigation are separate gates. Native tests and synthetic PPC instruction counts cannot establish physical Wii FPS.

## Measurement

FPS is emulated VBlank events per host second; OUTPUT is host presentation rate. Both can advance without new scene content. Core time includes rendering and device work, so it must not be labeled exclusively EE time. Compare real-Wii elapsed measurements, not Dolphin TimeBase or host test throughput, when claiming Wii speed.
