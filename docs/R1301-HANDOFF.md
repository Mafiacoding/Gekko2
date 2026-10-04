# Gekko2 R1301 — first CPU dynarec overhead reduction

The emulator is now named Gekko2. Launcher title, boot HUD, current log filenames and build outputs use the new name. Existing sd:/pcsx2/ BIOS, game and configuration paths are deliberately retained so the current SD card continues to work; historical PCSX2 references and internal symbols keep their meaning.

The EE native block emitter now uses a single relative PPC BL for aligned helper targets within the signed 26-bit branch range. Code buffers retain their address through finalization. Far/MEM2 or unaligned targets retain the established absolute CTR sequence. This removes three emitted PPC instructions per reachable callback while preserving all per-instruction preparation, source/TLB validation, Count/Compare, timers, interrupts and retirement calls. PPC ABI save/restore is unchanged.

The eight-instruction warm benchmark with the first instruction already prepared decreases from 3240 to 3195 PPC instructions (ADDIU), and from 3226 to 3181 (COP1 mix), compared with the R1300 ELF under the same synthetic platform services. This is about 1.4% fewer counted instructions in these specific blocks, not Wii cycles, FPS or overall BIOS performance. A fused retirement/preparation experiment was slower and is not in the delivered runtime.

This is a concrete extension of our own PPC emitter, not a Wii64/Lightrec core import. No foreign code was copied. Register residency across architectural boundaries, native guarded RAM blocks, branch linking and IOP block execution remain unfinished. GX rendering behavior stays at R1300.

Validation: 200 native tests; actual linked Interpreter/JIT PPC budget, complete ALU state, COP1 bit operations, self-modifying code, mapping replacement, all eight IRQ boundaries/EPC, source-page crossing, cache collisions, callee-saved registers and allocation retry. The synthetic PPC benchmark counts host instructions, not real Wii cycles or FPS. Full-VRAM signatures are compared between engines. Consult bundled verification for results.

Install the JIT DOL as apps/gekko2/boot.dol. Keep BIOS/disc/configuration data at its existing pcsx2 location. Log names are Gekko2-R1301-gx-render.log, Gekko2-R1301-software.log and Gekko2-R1301-first-fault.txt in that directory. Compare against R1300 with matching settings and send the SD log. No physical Wii speed or fresh BIOS cold-boot result is claimed.
