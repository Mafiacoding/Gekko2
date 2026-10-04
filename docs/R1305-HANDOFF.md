# Gekko2 R1305 — live mapped RAM blocks

2026-10-04. Early alpha. GX feature work remains paused; this checkpoint advances the custom PPC EE dynarec. It does not complete EE/IOP/VU recompilation or establish real-Wii performance.

## Implementation and safety

`ee_core_block_memory_resolve` returns physical RAM offset+1, or zero without state mutation. It shares the scalar translator's matching/page-selection logic via `ee_tlb_translate_selected`, exposing the selected EntryLo for conservative V/D admission. ASID and both-entry global rules remain unchanged. Stores require D; loads require V. Unsupported permission states decline to the existing scalar engine: this round does not repair or redefine scalar TLB exception behavior.

Virtual device dispatch precedes scalar TLB handling. The resolver excludes KUSEG at/above 0x10000000, masked MMIO windows 0x10000000–0x13ffffff, scratchpad and non-RAM physical results. KSEG0/1 direct aliases, proven ordinary KUSEG/KSEG2 mappings and the 16KB kernel mirror can use the native path. Pointer, alignment and subtraction-safe RAM bounds are checked; physical RAM is restricted to the PS2 32MB window.

Memory preparation validates the live source word/mapping, resolves current data, then prepares the guest instruction. The private emitter consumes the returned offset directly for little-endian byte/halfword/word accesses, correct signed/unsigned extension and preserved upper 64-bit lanes. Scalar translators remain available for unsupported accesses. No callback or retirement occurs between proof and access. No physical offset is cached across guest instructions.

The native prepared-memory function now uses `unsigned(st, first_prepared, first_physical_plus_one)`. The first already-fetched memory instruction is resolved before preparation and receives the proof in PPC argument r5. Other memory instructions get the proof from the live callback. Original ALU/COP1 callbacks, constant retirement exits, EABI preservation, source checks and per-instruction commit/interrupt checks remain.

The R1304 direct-only guard and preparation helper are retained for compatibility/tests; CPU integration uses the new resolved helper. Do not pass the old boolean callback to the native resolved-memory emitter.

## Validation

202/202 native tests; both devkitPPC r32/libogc 1.8.18 builds; Interpreter/JIT CPU and GS linked-PPC jobs. The same 96 mapped programs produce identical complete register/data digests on R1304 JIT and R1305 Interpreter/JIT. These exercise all eight memory families, signed offsets, KUSEG/KSEG2/kernel mirror and 4KB/16KB even/odd pages.

Tests additionally replace a live mapping after one instruction, revoke V/D/ASID/RAM/MMIO admission, overwrite the next source word through a mapped alias, and interrupt stores at all eight retirement boundaries. Inherited tests cover 64 direct-memory programs, scalar declines/fault-state parity, source mutation, 35 early source exits, all nonvolatile PPC registers, budgets and far callback fallback. ALU/COP1 signatures and 66 full-VRAM signatures match.

| Warm workload, eight instructions | R1304 JIT | R1305 JIT |
| --- | ---: | ---: |
| TLB-mapped RAM LW | 6458 | 4278 |
| Direct KSEG RAM LW | 3907 | 3766 |
| ALU | 3180 | 3185 |
| Mixed COP1/ALU | 3166 | 3171 |

Counts are actual linked PPC instructions with mocked allocator/cache/platform services, including guest retirement. They are not elapsed time, Wii cycles, whole-BIOS speed or FPS. Small ALU/COP1 setup overhead is reported rather than hidden. No fresh BIOS coldboot or physical GX/Wii run was performed for R1305.

## Reference and next work

The pinned nullDC4Wii review from R1304 remains applicable. Its helper-safe pinned SH4 register approach cannot be copied blindly: Gekko2 retirement may modify guest state at each boundary. No external code/engine was transplanted. Next work should audit those write contracts before register residency, add guarded wider memory/control paths and IOP blocks, and measure repeated real-Wii BIOS/OSDSYS behavior with the same settings.

Keep the single experimental GX setting and previous rendering/VRAM rules intact. Public source/checkpoint excludes BIOS, disc images, private guest RAM, owner logs and SDK binaries.

## Reproduce

Build with `tools/build_r1305.sh`, then run `tools/verify_regressions.py` and `tools/verify_ee_mapped_blocks_r1305.py <ELF> --nm <powerpc-eabi-nm>` for both engines. Run `tools/verify_gs_paths_r1285.py` for both engines. The mapped verifier also runs against the R1304 JIT ELF to establish the old/new digest and workload baseline. Verification JSON contains delivered binary SHA-256 values.
