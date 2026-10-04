# Gekko2 R1304 — first integrated EE RAM memory blocks

LB/LBU/LH/LHU/LW/SB/SH/SW now join ALU/COP1 operations in the real conservative 2–8 instruction EE block executor. This is an integrated runtime extension, not a standalone emitter demo. A live direct main-RAM proof precedes each memory instruction's preparation: nonnull RAM, KSEG0/KSEG1 alias, alignment and subtraction-safe range. The fetched-first path checks before advancing PC. Later unsupported addresses return the completed prefix so scalar execution retains MMIO/TLB/ROM/scratch/fault behavior.

The emitter chooses a memory-specific preparation callback only for memory instructions. The original ALU callback remains unchanged. Each instruction still validates live code/mapping and retires through the existing timer/Count/IRQ machinery. A store modifying the next source word stops stale execution. R1303 constant retirement exits, direct/far calls, Gekko2 artwork and GX behavior are retained.

Validation: 201 native tests; paired devkitPPC builds; four linked-PPC jobs. 64 mixed memory programs, 58 first-address declines, eight later unsafe families, SW source mutation, eight store IRQ boundaries and inherited 35 source exits/ABI tests pass. Full memory state/RAM signature matches old R1303 JIT and new Interpreter/JIT; complete ALU/COP1 signatures and 66 VRAM signatures agree.

Eight warm direct RAM LW instructions: R1303 5009 → R1304 3907 PPC instructions, about 22% fewer. Eight ALU/COP1 programs add three PPC instructions (3177→3180 / 3163→3166). Real guest retirement executes, but allocator/cache services are mocked. No physical Wii FPS, fresh BIOS cold boot or game performance claim.

nullDC4Wii's PPC register pinning, helper flush/reload, code checks and block dispatch were reviewed at commit 26a623e3feca9927d8ffe43706b06d82d672dd64. See NULLDC4WII-REVIEW-R1304.md for findings and source links. No foreign core/code/game fix or host-MMU fastmem machinery was imported. EE retirement helpers can observe guest registers, so a general allocator needs audited coherent flush/reload boundaries.

The remaining work includes guarded TLB data, wide loads/stores, a general register allocator, control-flow/linking and IOP blocks. Keep precise exceptions, delays, source invalidation and device interleave. Install JIT DOL as sd:/apps/gekko2/boot.dol; existing sd:/pcsx2/ data paths remain. The checkpoint excludes BIOS, discs, private RAM/logs and SDKs.
