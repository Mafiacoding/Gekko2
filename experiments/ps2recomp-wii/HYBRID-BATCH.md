# Native hybrid batch — 25 October 2026

Branch `codex/native-hybrid-cfg`. Runtime remains the existing Gekko2 core;
this batch adds an isolated offline compiler front end and integration tests.
It does not replace the emulator with the earlier 32-bit miniature prototype.
No BIOS/game input, generated owner code, snapshots or traces are included.

## Implemented

`hybrid_compile.py` reads ELF32 little-endian MIPS ET_EXEC. Starting from the
ELF entry or explicit entries, it follows direct calls/jumps, both conditional
outcomes and return sites. It includes legal delay slots, bounds the work, and
never invents indirect destinations or linearly scans unknown control flow.
CACHE deliberately stays at an interpreter boundary. A syscall's possible
continuation may be compiled, but only the runtime/handler selects the real PC.
Unknown code, unsupported slots and unmapped targets remain explicit gaps.

The output uses the **existing** guarded EE AOT generator and shared native
COP/MMI/memory semantics, compiled as ordinary C by the target compiler. It
uses the existing full EE state, including 128-bit GPR and HI/LO storage, SA,
FPU and VU state; it does not introduce a second incompatible 32-bit context.
It retains live instruction/mapping checks, precise memory helpers, retirement,
interrupt/event budgets, the interpreter, and the existing device backend.
No persistent PPC register ABI or save/restore mechanism changes here.

Generated files are private, outside this checkout, mode 0600 in a 0700 output
directory. Existing outputs are never overwritten. The immutable source/ELF
hashes and explicit coverage gaps are recorded privately; stdout contains only
counts and scope flags. The tool never downloads games, substitutes assets,
forces execution through a gap or promises ISO/CHD-to-playable-DOL conversion.

```sh
python3 experiments/ps2recomp-wii/hybrid_compile.py \
  --elf /private/owner/game.elf --output /private/hybrid-game \
  --max-blocks 512
# Optional authoritative code entries: --entry 0x... (repeatable).
```

This produces `ee_aot.c`, generated includes, a manifest and a report. It is an
**offline compilation stage**, not a standalone Wii linker/loader. Keep the
output private. Linking requires Gekko2's normal EE/device runtime and an
isolated include override, as demonstrated by `run_hybrid_test.py`; never copy
private generated includes into a public source checkout or source archive.
The 512-block default is a selection cap, not a measured Wii memory budget.

## Actual checks

```sh
python3 tests/test_native_hybrid_compile.py
python3 experiments/ps2recomp-wii/run_hybrid_test.py
python3 tools/verify_ee_aot.py --focused
```

- Five synthetic test methods pass: CFG edges, calls/returns, delay fallback,
  unknown flow, ambiguous backing, loops/limits, malformed ELF and private output.
- **512 real-core interpreter/AOT comparisons pass**, with nonzero native
  execution in each. A synthetic program executes AOT, returns to the actual
  interpreter for CACHE, and resumes another static region. Nonzero upper
  GPR lanes, four-lane MMI arithmetic, LQ/SQ, HI/LO/SA and memory agree with
  the interpreter across random values, 1/2/4/11-step budgets and a changed
  live delay-slot instruction. An independent model checks the stored words.
  The native retirement count is strictly below the total, proving fallback
  occurred rather than merely finding a registered native block.
- Existing focused checks pass: **4,716 EE**, **3,856 VU** and **255 IOP**
  comparisons, including source/partial exits, quantum, delay slots, IRQ/TLB/MMIO
  and VU pipeline cases. Other legacy suites are not rerun by `--focused`.

These are host C executions. No new Gekko2 DOL, linked PPC experiment, Dolphin
run, physical Wii test or FPS measurement is produced by this tools-only batch.
Existing compiled-PPC results from earlier reports are not re-labelled as new.

## Relationship to the Crash PS2Recomp work

The full PS2Recomp Wii runtime is already a form of static EE recompilation
plus a compatibility runtime. It has its own VU/IOP/software GS implementation.
It is **not yet linked to Gekko2's GX renderer or interpreter fallback**.
The new CFG tool instead exercises Gekko2's existing shared-state AOT path.
The two are deliberately separate until register/state, memory/MMIO, scheduler,
DMA and graphics handoff have an independently tested contract.

A separate PS2Recomp source batch corrects the R5900 SQRT.S source field (Ft,
not Fs). Local owner-private runs then show title -> loading -> corridor with
Crash and HUD. This is a correctness result, not evidence of Wii speed or a
finished native port. That branch retains its separate build/test report.

## Remaining work

- A validated shared-device bridge between the complete PS2Recomp runtime and
  Gekko2, especially VIF/GIF/GS/GX state, physical VRAM and scheduling ownership.
- Optional native function replacements with original-code identity, explicit
  ABI and alias/side-effect differential tests; names alone are insufficient.
- Dynamic/overlaid/self-modifying function coverage and unresolved indirect
  targets, per-game analysis/profiles, and a genuine library fallback contract
  for PS2Recomp (its missing-PC diagnostic is not a general EE interpreter).
- Automatic private disc extraction/build orchestration and a GUI; runtime
  compatibility cannot be inferred from container formats or static coverage.
- Complete graphics/audio/device semantics, matched native-path profiling,
  original Wii memory/runtime stability and sustained input/gameplay evidence.

HLE can reduce work for a validated service, but does not eliminate PS2 graphics,
VU computation, DMA or synchronization. Static recompilation removes translation
work; it does not make the original game workload free or guarantee 30 FPS.
