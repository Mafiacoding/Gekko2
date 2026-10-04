# Unreleased dynarec work after R1306

R1307 is an early-alpha development checkpoint. The owner has explicitly
requested ELF and checkpoint delivery after the register-residency work;
these test builds do not certify completion of the full dynarec plan.
See R1307-HANDOFF.md for the current verified scope and performance limits.

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
2. Wider IR-level allocation/residency coverage. Write-through scalar-source
   residency now crosses audited EE/IOP callbacks; opaque helpers and bodies
   remain fences, and patched inter-block residency is not implemented.
3. Native block links, source/TLB generations and invalidation/eviction tests.
4. Further IOP hardware-accuracy and HLE/context audit. Delayed loads, merge
   forwarding, synchronous EPC/BD/TAR, alignment and arithmetic overflow are
   now implemented and checked by independent host and linked-PPC oracles.
5. Remaining MMI/COP/VU flags, Q/P/EFU, pipelines, branches and GIF interactions.
6. Proven event batching and physical Wii coldboot/OSDSYS stability/profiling.

These are distinct substantial steps, not one combined final item. No promise
of 10 FPS, full game support or a completed dynarec follows from these tests.

Native warm chains also pass 126 paired budget/source/timer state comparisons,
a required actual cached successor check, direct thunk register preservation,
allocation failure fallback and 45 independent hostile-EABI oracle cases.
These results do not complete register allocation or VU/pipeline emulation.
The IOP block executor is described below; baseline parity is distinct from
fixing architectural limitations inherited from the scalar IOP core.

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

## IOP precise blocks and scheduler integration (unreleased)

The production scheduler now enters cached PPC IOP blocks, up to eight
instruction slots per block and eight scheduler ticks per host grant. Before
**every** actual IOP tick, it runs the same eight EE steps as before. It never
runs eight IOP instructions ahead of the EE. Explicit budgets also support
one tick, idle ticks, partial exits and nonmultiples of eight. Profiling is
still sampled at the same pseudo-random slice intervals.

Ordinary ALU, shifts, HI/LO, multiply/divide, all twelve integer memory forms,
COP0 transfers, ordinary branches/J/JR, JAL/JALR, REGIMM links and canonical
RFE have native bodies inside the generated block. JALR captures its target
before aliased link writes; REGIMM links follow the existing baseline's
link-before-condition convention. Expanded random J-target tests caught and
fixed an erroneous link write when target bits resembled SPECIAL/JALR.

SYSCALL/BREAK/traps, unsupported encodings and noncanonical special cases
use the original prepared scalar switch and end the block. The existing
retail-ROM classifier/embedded-ELF HLE call sites are checked live before
native execution. The complete pre-fetch HLE path, async queue, thread
scheduler, timer tick, vblank and IRQ retirement remain instruction-granular.
Previous-body retirement can share the next preparation callback, but occurs
**before** that next slot's EE grant. Budget/branch/source exits retire the
last native body before returning.

Formation peeks RAM or the existing BIOS address range, never speculative
MMIO. Each admitted slot verifies live source. A modified following word is
executed through scalar recovery within the already-consumed tick; stale
owned code is freed after the native function returns. Active execution pins
all IOP caches against reset. Init/shutdown reset owned code. The bounded
128-entry block cache allocates at most 512 KiB of code buffers, excluding
allocator metadata and the existing single-op cache. Cache hashing mixes
instruction and eight-word block positions to avoid sequential-block bias.
SD logs now include `IOP_BLOCK compiled=... runs=... ticks=... stale=...`.
Ticks include HLE/idle/recovery; they are **not** a native opcode count.

Verification:

- 208/208 native host tests, including a new tick-budget/idle/halt test.
- 477 paired linked-PPC block/scalar programs with full IOP state and memory
  comparison: integer classes, controls/delay slots, COP0/RFE, exceptions,
  IRQ/idle, HLE, source mutation and allocation failure.
- Exact EE8/IOP1 pre-fetch observations for 1/7/8/9/17/257 slices, crossing
  native block boundaries and profiling intervals. Both near MEM1 and far
  generated-code calls are exercised across the focused tests.
- Warm entry replacement, reset pinning and synthetic retail-ROM HLE guards.
- 245 independent hostile-EABI wrapper oracles, including budget zero,
  consumed/unconsumed early exits, scalar termination, nonvolatile registers,
  CR2..4 and caller linkage/argument-area writes.
- 16,000 independent arithmetic cases, 14,000 control/divide/link cases and
  4,800 memory/helper cases across all twelve integer memory forms.

The linked tests execute the real cross-built PPC code with mocked platform
allocation/cache/console services. Synthetic instructions/ROM words are used;
no BIOS/disc is included. They do not prove physical Wii coldboot stability,
10 FPS or physical-Wii pipeline behavior. At the time of this initial block
verification, the scalar baseline lacked load-delay state and complete
synchronous-exception BD handling. The subsequent precision work below
repairs these specific gaps and adds independent architectural oracles.

### Earlier warm IOP instruction counts (before load-delay precision)

Eight guest retirements in the current cross-build, comparing its public
single-step scalar/JIT dispatch with its warm precise block path. These
counts exclude compilation and are **not** elapsed cycles, before/after
public-release speed, whole-BIOS timing or Wii FPS.

| Workload | Single-step path | Block path | Change |
| --- | ---: | ---: | ---: |
| ADDIU | 5200 | 5452 | 4.8% more |
| ORI | 5472 | 5452 | 0.4% fewer |
| MULT | 6456 | 5476 | 15.2% fewer |
| DIV | 7307 | 5580 | 23.6% fewer |
| LW | 8164 | 7172 | 12.2% fewer |
| SW | 8338 | 7340 | 12.0% fewer |
| LWL | 8321 | 7276 | 12.6% fewer |
| RFE | 6473 | 5500 | 15.0% fewer |

Simple ADDIU still costs more, so a universal IOP speedup is not claimed.
General register residency, audited fast boundaries, code generations and
whole-system hardware profiling remain follow-up work. This completes the
bounded IOP block execution path against the existing baseline, not the
entire dynarec completion plan. R1306 remains the delivered release.


## IOP pipeline precision and general word allocation (unreleased)

The IOP now queues load/MFC0 writes for one instruction. The next instruction
reads the old visible GPR value; explicit writes and a replacement load to
the same register cancel the older pending write. LWL/LWR forward pending
merge data while computing addresses from the visible base, including
base/destination aliasing. Single-op and block JIT dispatch decline to the
precise scalar merge path when this forwarding is required. Pipeline fields
are appended after the original JIT state prefix.

An explicit branch-delay flag includes untaken conditional branches.
Synchronous exception entry captures EPC, Cause.BD, TAR and BadVAddr before
redirecting execution, applies the Status exception stack and chooses the
BEV vector. Half/word alignment and signed ADD/ADDI/SUB overflow are checked
before side effects, including a zero destination. Fetch alignment faults
commit an older pending load without executing a guest instruction. IRQ
entry exposes the pending load. Thread contexts save/restore pending state;
recognized HLE gates flush it before reading arguments and decline without
flushing unrelated addresses.

Our generated HLE syscall trampoline now includes explicit delay NOPs after
loads/MFC0, with recomputed local branches. The existing default HLE policy
for skipping unhandled synchronous traps resumes at TAR when the trap was
in a delay slot. This is an explicit HLE policy, not a replacement for a
complete real IOP kernel/exception dispatcher.

The new EE/IOP allocator binds individual architectural words to existing
PPC temporary registers r4-r11, reuses resident loads and defers dirty stores.
It spills before temporary reuse, helpers, control-flow entries/edges and
observable exits. Relative local branches are relocated after transformation.
Unknown register effects conservatively end tracking, including update-form
loads; any write to r3 invalidates the context-pointer assumption. Unsupported
branch shapes or allocation failure leave the original body intact. The
prepared-first EE body is conservatively left unchanged.

This is a general machine-word binding/spill pass over existing emitted
bodies, not yet an IR liveness allocator with cross-instruction/helper
residency. Full 64/128-bit state uses the original word offsets. All current
preparation/retirement callbacks remain observable, so the pass flushes
there. No event checks were removed. SD logs distinguish compilation counters
for accepted bodies, deleted instructions, reused memory loads and spills;
these counters do not measure executed guest work or FPS. A load replaced
by a register move saves a memory read even when instruction count is equal.

Verification includes the complete 209-test host suite; independent
linked-PPC pipeline and allocated integer/alias oracles; 477 paired IOP
programs with exact EE8/IOP1 ordering; 245 hostile-EABI IOP wrapper oracles;
1,152 merge byte-lane cases with 64 exact IRQ exits; 126 native successor
comparisons; 552 COP1 and 56 link/BC1 outcomes; and 30/68 helper plus 45
continuation EABI oracles. Platform services are mocked. No physical Wii
coldboot/OSDSYS stability or speed improvement is established by this work.
R1306 remains the delivered release; the completion gate is still open.


## R1307 audited register residency

Frequently reused scalar operand words use r18-r29 across instruction and
prepare/retire boundaries. r30 retains the external-GPR generation. Temporary
operands are renamed to those bindings so resident loads disappear rather
than becoming extra moves. Stores keep the guest register file canonical.
Overwriting a resident word first materializes any live temporary aliases of
its previous value. All pool registers, stack and CR2-4 follow PPC EABI.

EE preparation changes r0 and pipeline/proof state, not nonzero GPRs. The
retirement helper closure changes nonzero GPRs only through the audited HLE
thread context replacement, which increments the generation. IOP retirement
can restore/mature delayed loads; IRQ entry, thread replacement and Alarm
dispatch also increment the IOP generation. The live guard after preparation
reloads valid copies when the generation differs. Counter wrap is tested.

Unknown helper/register effects, local control-flow bodies and partial-word
writes fence residency. Generic callback emitters remain conservative; the
frontends opt into dedicated resident APIs with these explicit contracts.
Memory-only blocks and unsuitable sources do not allocate a resident pool.
The scalar pool covers low 64-bit EE operands and full IOP words; upper EE
words remain canonical and are preserved by the existing word allocator.
This completes the bounded audited scalar residency path, not full IR
liveness allocation, arbitrary-helper contracts or the remaining dynarec.

The 62 independent observer/external-write/generation-wrap programs and 48
hostile resident-pool ABI programs pass, alongside 211 host tests, including
real EE/IOP context-replacement and Alarm invalidation checks. The handoff
reports warm PPC instruction counts without hiding regressions. No physical
Wii speed or BIOS stability claim follows from synthetic execution.
