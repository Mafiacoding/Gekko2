# nullDC4Wii reference review — Gekko2 R1304

Reviewed 2026-10-04, nullDC4Wii main commit `26a623e3feca9927d8ffe43706b06d82d672dd64`. These are design references, not imported code, game patches or evidence of Gekko2 hardware speed.

Sources:

- https://github.com/BenoitAdam/nullDC4Wii/blob/26a623e3feca9927d8ffe43706b06d82d672dd64/wii/dc/sh4/rec_v2/wii_driver.cpp
- https://github.com/BenoitAdam/nullDC4Wii/blob/26a623e3feca9927d8ffe43706b06d82d672dd64/dc/sh4/rec_v2/blockmanager.cpp
- https://github.com/BenoitAdam/nullDC4Wii/blob/26a623e3feca9927d8ffe43706b06d82d672dd64/dc/sh4/rec_v2/driver.cpp

## What transfers conceptually

The Wii driver keeps selected SH4 GPRs in callee-saved PPC registers and flushes/reloads around interpreter/context-changing operations. Its comments explain why ordinary memory helpers can preserve pinned registers while interrupt/context handlers require coherence. Gekko2 cannot assume this split yet: EE retirement can invoke helpers/interrupt handling that observe or modify guest registers after every instruction. Persistent EE register allocation therefore needs an audited helper/event contract rather than unconditional pinning.

The driver validates compiled guest source to protect against code overlays and includes PPC-MMU fastmem paths with matching fallback patches. Gekko2 retains stronger per-instruction live source/mapping checks in its conservative executor. The R1304 native RAM proof is an independent implementation using software bounds/alignment/segment checks; it does not install the nullDC host-MMU/exception machinery.

The block manager uses cached guest-address/code pairs and bounded buckets to reduce dispatch overhead. Cache/entry lifetime remains important whenever containers, code storage or mappings change. Gekko2 keeps its fixed slot array and does not release an executing native buffer. No foreign block-manager implementation was copied.

## Concrete R1304 work

Eight byte/halfword/word load/store families can now join ALU/COP1 instructions in the actual precise EE block executor. Live KSEG0/KSEG1 addresses must prove aligned, nonnull main RAM and subtraction-safe bounds before guest preparation. Unsupported addresses decline into the existing scalar path without performing the memory operation or retiring that instruction. The emitter chooses a memory-specific preparation callback only for memory instructions, preserving the existing ALU source-check path.

This is an integrated memory-block milestone, not a complete JIT, branch linker, general register allocator or IOP block engine. GX is unchanged. Dreamcast performance figures and SH4 game fixes do not predict PS2 performance or compatibility.
