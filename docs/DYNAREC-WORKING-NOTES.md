# Unreleased dynarec work after R1306

This is source development, not an alpha release or a completed full dynarec.
R1306 remains the last delivered build. The owner requests ELF/DOL/checkpoint
only once genuinely at most one planned step remains; that gate is not met.
Internal cross-builds exist solely for verification.

Implemented in the working source:

- Compile one legal ALU/COP1 or proven RAM delay slot after selected controls.
  The callback checks live source and pending branch state before advancing PC.
  Annulled, modified, unsafe or unsupported slots fall back before execution.
- Extend REGIMM links and BC1 controls. BLTZAL/BGEZAL were absent from the
  interpreter. Link-before-condition ordering also handles source register 31.
- Guarded continuation after a native function returns, inside the original
  EE budget. Warm cached successors now dispatch through a native PPC thunk;
  cold successors retain the C compilation path. The resolver never allocates
  or evicts while a chain is active. This is not patched native tail linking
  or a generation cache. The 8 EE : 1 IOP scheduling boundary is retained.
  The SD EE_BLOCK record includes native_successors for hardware diagnosis.
- Native proven LWL/LWR/SWL/SWR/LDL/LDR/SDL/SDR merge paths select byte
  lanes directly in RAM and preserve upper registers. All 1,152 programs
  match the scalar state and an independent byte oracle; aliased source
  changes and 64 exact IRQ exits are covered.
- Proven LWU/LQC2/SQC2 RAM paths. Vector transfers preserve the unmasked EA;
  TLB page-crossing cases stay scalar. VF00 loads have no access; stores use
  the architectural (0,0,0,1) constant despite dirty VF00 backing.
- Admit existing COP1 arithmetic emitters into precise blocks after comparing
  their complete GPR/COP0/FPR/FCR31/ACC state with the scalar engine.
- Fix SQRT.S/RSQRT.S masks stored at SP+4 across libm calls. A callee may use
  that location for LR. Move live spills and nonvolatile saves above the
  linkage/register argument area, also in VU square-root, conversion and
  scalar memory/MMI helper trampolines. The IOP memory frame likewise moves
  its r14-r17 saves above the callee argument area.

Verification completed for this work includes 256 delay-slot programs,
60 LWU/vector programs, seven continuation budgets plus scalar fault fallback,
552 COP1 corner programs, 56 additional link/BC1 outcomes and direct native
CVT.S.W EABI preservation. A hostile helper oracle separately covers 30
floating/VU conversion/root calls and 68 scalar memory helper cases, including
volatile register and linkage/argument-area writes. The final internal linked-PPC comparison passes all 10 paired EE signatures
and 66 complete GS VRAM signatures. The IOP memory oracle passes 4,800
cases with hostile linkage/argument writes, alongside 16,000 existing ALU
and 10,000 control/divide cases. Earlier partial logs are not a release
gate. These synthetic PPC executions use mocked platform services and are
not Wii timing, stable BIOS frames, PS2-wide floating-point correctness or FPS.

## Remaining actual work

1. Remaining EE instruction/trap/control cases and instruction-family audit.
   The eight standard word/doubleword merge RAM operations are implemented.
2. General register allocator and residency with audited helper read/write
   contracts, dirty state flushing and reload on every observable boundary.
3. Native block links, source/TLB generations and invalidation/eviction tests.
4. IOP block executor without running ahead of the current EE interleave.
5. Remaining MMI/COP/VU flags, Q/P/EFU, pipelines, branches and GIF interactions.
6. Proven event batching and physical Wii coldboot/OSDSYS stability/profiling.

These are distinct substantial steps, not one combined final item. No promise
of 10 FPS, full game support or a completed dynarec follows from these tests.

Native warm chains also pass 126 paired budget/source/timer state comparisons,
a required actual cached successor check, direct thunk register preservation,
allocation failure fallback and 45 independent hostile-EABI oracle cases.
These results do not complete register allocation or the IOP/VU phases.

## Helper contracts before register allocation

| Boundary | Observed contract | Consequence |
| --- | --- | --- |
| Ordinary preparation | Live source/control checks; reset r0 and transient exception fields; advance PC | Decline must expose old architectural state |
| Resolved memory preparation | Live TLB, V/D, ASID and physical RAM proof before ordinary preparation | Never cache the proof across a retirement callback |
| Delay preparation | Verify pending branch/source, prove RAM if needed, capture BD, consume target | No normal preparation shortcut for a delay slot |
| Retirement | Count, IRQ latch/take, vblank, boot/menu/SBUS heuristics, GS/timers and device work | Guest registers/context may be read or changed; PPC callee-save alone is insufficient for residency |
| Native helper call | r3-r12, f0-f13 and caller linkage/argument area are volatile | Keep private state above the callee area; restore context and nonvolatile registers |
| Returned-block continuation | Full native function has returned before cache lookup/compilation | No live return address in an evictable generated allocation |

## Focused instruction-count measurements after merge/native-chain work

All numbers are actual linked PPC instructions for eight warm guest
retirements, with mocked platform services. They are not elapsed Gekko cycles,
whole BIOS timings or Wii FPS. The comparison is against the internal build
before these merge/chain changes, not the older public R1306 binaries.

| Workload | Before | After | Change |
| --- | ---: | ---: | ---: |
| LWL | 5967 | 4053 | 32.1% fewer |
| LWR | 6007 | 3973 | 33.9% fewer |
| SWL | 7639 | 4029 | 47.3% fewer |
| SWR | 7623 | 3957 | 48.1% fewer |
| LDL | 5854 | 4093 | 30.1% fewer |
| LDR | 5894 | 4117 | 30.1% fewer |
| SDL | 6487 | 4093 | 36.9% fewer |
| SDR | 6535 | 4117 | 37.0% fewer |
| Jump then ALU | 3460 | 3447 | 0.4% fewer |
| Jump cycle | 3881 | 3728 | 3.9% fewer |
| Plain ALU | 3228 | 3244 | 0.5% more |

The plain ALU overhead is retained in this record rather than claiming that
all workloads improve. The native host suite now passes all 207 tests.
