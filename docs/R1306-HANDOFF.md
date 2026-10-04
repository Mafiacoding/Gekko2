# Gekko2 R1306 — wide memory, terminal control and delay-slot correctness

2026-10-04. Early alpha. This checkpoint advances two full-dynarec steps; general register allocation, linking, IOP blocks and VU completion remain open. See DYNAREC-COMPLETION-PLAN.md for implementation and readiness gates. GX feature work remains paused.

## Implementation

Six native memory families join conservative 2–8 instruction EE blocks: LD/SD (64-bit), LQ/SQ (128-bit), LWC1/SWC1 (raw FPR bits). The R1305 live physical-offset proof is reused. Width/alignment/bounds are checked before preparation; LQ/SQ effective addresses are rounded down to 16 bytes. LQ $zero is admitted as a no-access operation, not a RAM load. LWC1 $f0 is a real FPR write. LD preserves the upper 64-bit lane; LQ updates both lanes. The emitter retains the original proven address for rt==rs.

An explicit memory-store policy replaces opcode-range guesses: SQ is a store although its opcode is low, while LD/LWC1 are loads despite high opcodes. Loads need V admission, stores need V/D. Unsupported mapping/MMIO/ROM/scratch/alignment/bounds cases decline into the existing scalar engine. Scalar TLB permission/fault modeling is not redefined by this round.

J/JAL/JR/JALR, BEQ/BNE/BLEZ/BGTZ and their selected likely/REGIMM forms can terminate a block. Formation stops at that instruction; the existing scalar frontend executes or annuls the delay slot. No direct block links or fused delay-slot execution are claimed. Live source checks and per-instruction retirement remain.

## Correctness fix found during extension

The shared native regular-branch emitter previously conditioned branch_pending on the taken mask. An ordinary not-taken branch still has a delay slot. This flag matters for deferred IRQ delivery and EPC/BD on a fault in the following instruction. The shared emitter now always sets the flag for regular branches, while conditioning only next_pc. Likely-branch annul logic remains separate.

The expanded old/new/interpreter comparisons confirmed eight prior scalar REGIMM cases differed from the interpreter only in this delay flag and following delay-context byte. R1306 matches the interpreter in all 128 new control programs, including those eight corrections. The block executor had also exposed the same shared-emitter issue for regular BEQ/BNE/BLEZ/BGTZ, where old scalar frontend routing had used C. This was fixed before release.

## Verification

203/203 native tests; both devkitPPC r32/libogc 1.8.18 builds; four linked-PPC CPU/GS jobs. New tests include 72 wide/FPR memory programs across direct and TLB RAM, zero/overlapping base destinations and NaN bit preservation; 128 terminal controls; 48 wide-memory IRQ positions; taken/not-taken branch IRQ deferral and TLB faults in the delay slot; an independent scalar REGIMM delay check. Inherited direct/mapped memory, code-source alias mutation, source/TLB replacement, budgets, EABI and far callbacks remain tested. All 66 Interpreter/JIT full-VRAM signatures match.

The expanded test explicitly initializes all COP0/FPR/transient registers before each case. The original harness only reset the GPR-focused subset; digesting additional COP0 registers without resetting them leaked an earlier JIT-only IRQ's unrelated EPC into the comparison. That test setup was corrected before comparing complete state. The actual delay-flag mismatch remained and led to the real emitter fix above.

Wide-memory state matches R1305 JIT and R1306 Interpreter/JIT. Control state matches R1306 Interpreter/JIT; the old scalar REGIMM bug is recorded as corrected, not misrepresented as unchanged baseline parity.

## Measured workloads

Warm counts below cover eight actual guest retirements under the linked PPC harness with mocked allocation/cache/platform services. They are not hardware cycles, BIOS elapsed time or Wii FPS.

| Workload | R1305 JIT | R1306 JIT |
| --- | ---: | ---: |
| LD | 5590 | 4353 |
| SD | 6286 | 4345 |
| LQ | 7054 | 4358 |
| SQ | 7366 | 4369 |
| LWC1 | 5974 | 4329 |
| SWC1 | 7302 | 4337 |
| direct_LW | 3766 | 3769 |
| TLB_LW | 4278 | 4321 |
| ALU | 3185 | 3184 |
| COP1_mix | 3171 | 3168 |

New wide/FPR workloads use 22.1–40.7% fewer PPC instructions. Existing direct LW is +3, TLB LW +43 (about 1.0%), ALU -1 and mixed COP1 -3. Report these small regressions alongside improvements. No fresh BIOS coldboot or physical-Wii/GX timing was performed.

## Next work

Audit helper/retirement guest-register write contracts before integrating liveness/dirty-value allocation. Then fuse legal branch delay slots, remaining memory/control cases, invalidation-safe links and IOP blocks. Remaining MMI/COP/VU pipeline/flags/Q/P/EFU/XGKICK cases need exact differential tests and verified fallback. The nullDC4Wii R1304 review remains a design reference, not an imported engine.

## Reproduction/distribution

Run tools/build_r1306.sh, tools/verify_regressions.py, tools/verify_ee_extensions_r1306.py <ELF> --nm <powerpc-eabi-nm> and tools/verify_gs_paths_r1285.py for both engines. Run the extension verifier on the old R1305 JIT ELF for baseline memory counts and the recorded scalar delay-flag discrepancy. Public verification JSON includes binary hashes. The checkpoint excludes BIOS, discs, real guest RAM, owner logs and SDKs. Mocked test contexts are synthetic.
