# Gekko2 + PS2Recomp: Wii feasibility track

24 October 2026. This is an isolated **integer-leaf probe**, not a Wii game
runtime. Gekko2 continues separately as the general emulator. Runtime source
remains `e7aae58`; production EE/GS/GX behavior is unchanged by this tool batch.

Pinned upstream: [ran-j/PS2Recomp](https://github.com/ran-j/PS2Recomp), commit
`e5636981a1c9de368f6b71e819c3dc188dfedbf4` (GPL-3.0). No upstream sources or
private generated game code are bundled here. Its host recompiler/analyzer build
successfully with GCC 14.2, CMake 4.4.4 and Ninja 1.13.2 on this Linux workspace.

## First results

- New `tools/ee_recomp_oracle.c`: isolated function-call context using Gekko2's
  actual interpreter and ELF loader. Fixed test stack/return sentinel and a flat
  two-page 32-MiB TLB mapping; interrupts disabled. This is not BIOS boot,
  syscall emulation, a state captured from a game, or a progression patch.
- Own ELF fixture: real symbols and R5900 ADDIU/ADDU/XOR/SLL/JR/delay-slot code.
- Actual pinned PS2Recomp generates the C++ file without changing instructions,
  configured stubs, skip lists or CACHE/COP0/SYSCALL patching.
- A test-only scalar adapter lets that unchanged generated leaf compile on
  x86-64 and **32-bit big-endian PPC750**. Both execute and agree with Gekko2 for
  **76 cases**, including overflow/bit-pattern edges and deterministic random
  inputs. Compare all GPR storage, nonzero upper-lane preservation, HI/LO, SA,
  return PC and delay-slot result; verify PPC callee-save registers and SP.
  An independent arithmetic model checks the synthetic return value.
- Five oracle test methods pass (38 arithmetic inputs, upper-lane preservation, budget exhaustion,
  reserved-instruction handling and 10 malformed-input/argument cases).
- Existing CLIP and VI branch regression executables pass again.
- A nontrivial private function from the owner's Crash E3 ELF also passes
  **76 host/PPC/Gekko2 comparisons**. Another 72 inputs match an independent
  IEEE finite-value model. This private function and all generated source,
  ELF, input image and raw output remain outside the repository/artifacts.

The public synthetic record is in `validation/synthetic-report.json`. It records
hashes of that run, not hardware performance. No physical Wii or Dolphin run.
There is **no new emulator DOL or game DOL** in this batch.

## What the adapter does and does not do

`leaf_adapter/` implements only a small scalar integer interface used by the
probe. Its context is **not ABI-compatible with the full upstream runtime or
Gekko2 state**. Comparison exchanges explicit architectural values. Nonzero
upper GPR lanes test preservation, not 128-bit MMI arithmetic or chain residency.

The adapter contains no success-returning syscall/device/dispatch stubs. A leaf
requiring omitted calls or intrinsics must fail to compile. The dedicated
`GEKKO2_PS2RECOMP_LEAF_PROBE` guard prevents accidental normal runtime inclusion.
Do not wire this test adapter into Gekko2 gameplay or present it as a complete
PPC backend. HI/LO upper lanes remain zero in these probes; no memory, FPU, VU,
MMI, exceptions inside translated functions, asynchronous events or IRQ timing
are validated by the generated-leaf comparison.

## Reproduce

Use the current official devkitPro/devkitPPC/libogc for Wii development; this
probe used devkitPPC r50/GCC 16.1.0. The standalone probe ELF does not link libogc
and is not an HBC application. Actual emulator builds retain the existing
SPEED/AOT/DSI/GX flags and review gates.

Build the **host tools**, not the PC graphics runtime:

```sh
# /workspace/PS2Recomp is a separate checkout pinned to the commit above.
cmake -S /workspace/PS2Recomp -B /workspace/ps2recomp-build-host -G Ninja \
  -DPS2X_BUILD_RUNTIME=OFF -DPS2X_BUILD_TEST=OFF -DPS2X_BUILD_STUDIO=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /workspace/ps2recomp-build-host -j 3
```

Dependency revisions observed in that build:

| Dependency | Commit |
|---|---|
| ELFIO | `8ae6cec5d60495822ecd57d736f66149da9b1830` |
| toml11 | `be08ba2be2a964edcdb3d3e3ea8d100abc26f286` |
| fmt | `407c905e45ad75fc29bf0f9bb7c5c2fd3475976f` |
| libdwarf | `2e088b983562860f497b4fd789a3e61524eb857c` |
| rabbitizer | `d5804fa488474830a47fbf9f3693048ec1e45589` |
| nlohmann/json | `9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03` |

From the Gekko2 root, using Python with Unicorn 2.1.4:

```sh
python3 tests/test_ee_recomp_oracle.py
/workspace/.venv-gekko2/bin/python experiments/ps2recomp-wii/run_probe.py \
  --upstream /workspace/PS2Recomp \
  --recompiler /workspace/ps2recomp-build-host/ps2xRecomp/ps2_recomp \
  --output /workspace/recomp-lab/verified-synthetic \
  --devkitppc /workspace/.devkitpro/devkitPPC
```

The runner rejects generated output inside the source checkout, a different
upstream commit and tracked upstream edits. It leaves generated C++ unchanged,
builds a freestanding PPC leaf ELF, executes its instructions in Unicorn, and
compares with the actual Gekko2 oracle. Runtime heap/device services are absent.
This small artifact is **not** checked by the production emulator MEM1 gate;
it does not establish capacity for a complete game/runtime.

Optional owner-private probes require all of `--private-elf`, `--function`
(generated identifier) and `--entry` (guest address). Output, code and logs for
that mode must stay private. The same integer-only adapter and strict return
checks apply. The runner does not discover a safe leaf or provide input memory
for a caller; deliberately choose a bounded, independent function.

## Actual full Crash code generation: limits

A separate private run with the owner's complete ELF, empty configured stubs and
skip lists, and all three patch flags OFF completed its code-generation pass:

| Upstream report | Count |
|---|---:|
| Functions discovered | 2,847 |
| Functions translated to ordinary generated bodies | 2,582 |
| Runtime/stub wrappers selected by upstream classification | 265 |
| Skipped / decode failures / reported unhandled instructions | 0 / 0 / 0 |
| Additional entrypoints | 47,540 |
| Fallback promotions / fallback entries | 454 / 69,748 |
| Warnings / errors | 454 / 0 |

These are upstream generator counters, **not complete instruction semantics,
function-boundary proof or a working port**. Wrapper count is not a claim that
265 services are missing: each actual runtime handler still needs audit.
The roughly 66-MiB generated source tree remains private. It was not linked into
a full PPC game executable. Generated source size is not executable size.

## Two next batches

**Gekko2:** use these independently checkable EE cases and existing profiler
scopes to investigate hot paths, native admission/fallback causes and event
boundaries. Extend to memory/MMI/FPU with explicit oracles; retain the proven
VU fixes, original interpreter, software GS, mapping guards and scheduler.
The long software-float sine initialization still needs native-path profiling.
No game-specific precomputed table or skipped initialization.

**Wii recompilation:** expand only demonstrated portable operations; implement
an architectural state adapter, guest-endian memory access and bounded
retirement/events before attempting real function calls inside Gekko2. Then
choose whether the generated code can share Gekko2's device backend, rather than
porting an entire second PC runtime. A scoped GS/GX bridge, VU timing, IOP/SIF,
DMA/IRQ behavior and memory/code-size budgets remain required.

An official Wii backend is not present in the reviewed upstream checkout.
Its normal headers use x86 intrinsics or an ARM SSE2NEON alternative; neither
is a Broadway SIMD implementation. A scalar test leaf succeeding does not solve
those vector paths. Native speed benefit remains unmeasured. BIOS tiles,
rare GX timeout, other games and sustained Wii gameplay remain open.

## Guarded native-hybrid CFG follow-up (25 October)

[Implemented compiler stage, real-core handoff tests and remaining boundaries](HYBRID-BATCH.md).
The offline ELF front end reuses Gekko2's existing 128-bit state, AOT and
interpreter rather than introducing a second minimal CPU. 512 synthetic mixed
AOT/interpreter comparisons pass. It is not a complete PS2Recomp/Gekko2 device
bridge, standalone game converter, new emulator DOL or Wii speed proof.
