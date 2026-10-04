# Gekko2 R1307 — precise blocks and audited scalar register residency

Development checkpoint with requested JIT and Interpreter test binaries.
This revision includes all work on the development branch after R1306;
the complete EE/IOP/VU dynarec is not finished.

## What changed

- Precise bounded IOP native blocks preserve the exact EE8/IOP1 scheduling
  order. Every source/proof/HLE/IRQ boundary remains observable.
- IOP loads and MFC0 have delayed writes. LWL/LWR forward pending merge
  data while preserving the visible address base, including aliases.
- Synchronous exceptions capture taken/untaken delay slots, EPC/BD/TAR,
  BEV, BadVAddr, alignment and signed arithmetic overflow before effects.
  Our own HLE syscall trampoline and trap resumption honor these hazards.
- EE work includes legal delay-slot fusion, link/BC1 controls, RAM merge
  and vector transfers, COP1 block admission and native warm continuation.
  Helper linkage/argument-area spills were corrected.
- Word binding and spills work inside emitted bodies. Frequently reused
  scalar sources now survive audited helpers in r18-r29; operand renaming
  removes resident loads. All guest stores remain visible in memory.
  Generation changes refresh copies after thread/context replacement,
  pending IOP loads, IRQ entry or Alarm argument changes. Unknown effects,
  partial writes and unsuitable bodies fence this optimization.

The resident path supports ordinary scalar words and low 64-bit EE operands.
Upper 128-bit words remain canonical and are preserved. It is not a general
IR liveness allocator or residency across arbitrary/unknown helper contracts.
The generic emitter APIs retain conservative behavior. Native successor
dispatch is a returned-function chain, not patched native tail links.

## Verification

- 211 host tests, including actual EE/IOP context replacement, Alarm
  invalidation and generation wrap.
- 62 independent canonical-helper-view, external-GPR mutation, destination
  alias and generation-wrap programs; 48 hostile resident-pool EABI cases.
- 245 IOP wrapper budget/early-exit/EABI cases; 477 paired linked-PPC IOP
  programs with exact EE8/IOP1 observations, source mutation and warm guards.
- 50 independent IOP pipeline/exception oracles; 1,152 independent merge
  byte-lane cases and 64 exact IRQ exits; native successor/budget comparisons.
- Broad EE instruction-family checks, 552 COP1 and 56 link/BC1 outcomes,
  matching Interpreter/JIT signatures; helper and continuation ABI checks.

Actual cross-built PPC runs with mocked allocation/cache/console services.
Programs and ROM words are synthetic. There is no fresh physical-Wii BIOS
boot, sustained OSDSYS navigation, game-performance or FPS result for R1307.

## Warm instruction counts

Eight fully retired guest instructions, same synthetic inputs, comparing
the pre-residency development JIT with R1307. Compilation is excluded.
These are PPC instruction counts, not hardware cycles or FPS. Avoiding
memory operand loads can still add generation checks and register saves.

| Workload | Before | R1307 |
| --- | ---: | ---: |
| EE ADDIU accumulator | 3244 | 3244 |
| EE OR repeated sources | 3287 | 3292 |
| EE XOR accumulator/constant | 3287 | 3302 |
| EE LW base | 3893 | 3893 |
| IOP ADDIU accumulator | 6600 | 6590 |
| IOP OR repeated sources | 6632 | 6637 |
| IOP XOR accumulator/constant | 6632 | 6642 |
| IOP LW base | 8317 | 8335 |

Some workloads count more instructions. No universal speedup is claimed.
Real Wii logs must determine whether removed operand loads outweigh the
additional guards, and which workloads need further admission tuning.

## Wii installation and logs

Copy `Gekko2-R1307-JIT.dol` into the existing HBC app directory as `boot.dol`.
Use the supplied R1307 `meta.xml` and existing Gekko2 icon. Existing
`sd:/pcsx2/` BIOS, settings and game-image paths remain in use.
`Gekko2-R1307-JIT.elf` is the matching ELF for ELF loaders/debugging.
An Interpreter pair is included in the checkpoint for comparison.

Logs use `sd:/pcsx2/Gekko2-R1307-gx-render.log` or
`sd:/pcsx2/Gekko2-R1307-software.log`, depending on renderer selection.
`WORD_ALLOC` and `RESIDENT` are compilation counters, not measured FPS.
The first-fault dump is `sd:/pcsx2/Gekko2-R1307-first-fault.txt`.
Binary emulator savestates from older struct layouts may be rejected;
the checkpoint archive is the source/build checkpoint, not a PS2 savestate.

## Remaining work

Wider EE/MMI/COP coverage and trap audit; remaining VU flags/Q/P/EFU/pipeline
and GIF behavior; patched links and source/TLB generations; wider allocation
and proven event batching; repeated real Wii BIOS/GX boot and FPS profiling.
No BIOS or disc image is distributed with this checkpoint.
