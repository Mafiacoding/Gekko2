# Wii U port: the GS on the GPU, and what limits speed first (2026-11-23)

Status: analysis plus one measurement tool. No GPU rasterizer exists in the Wii U port; the GX2 code only presents the
finished picture (`wiiu/gx2/`, run by the owner in Cemu 2.6 at 61 frames/s).

## What the Wii build does today (from the repository, not from a run)

`source/hw/gs_gx.c` (about 2,000 lines) draws selected GS primitives with GX (flat sprites and triangles, guarded
texture snapshots, Gouraud/UV/STQ cases, a few exact blend and depth cases) into the EFB and copies the result back into
the software GS VRAM, with ownership rules (`docs/GX-PRESENTATION.md`). The setting "GX (EXPERIMENTAL)" is **off by
default**, and the docs say earlier logs cannot establish primitive coverage or speed. So even on the Wii the GPU path is
unproven as a speed gain.

## What a GX2 port needs

1. A GX2 backend for each primitive family above (flat, textured, Gouraud, STQ), each with pixel shaders written in Latte
   assembly or GLSL and assembled with `latte-assembler` (the owner has it now).
2. PS2 VRAM is swizzled, so textures and results go through the existing pack/unpack code (`gs_gx_pack_*`,
   `gs_gx_import_*`) with a CPU readback per resident region.
3. The same VRAM-ownership rules, barriers and fallbacks as the Wii path, plus exact blend/depth/alpha-test cases
   (`docs/BETA12.20-GX-ALPHA-RESIDENCY-PLAN.md` lists what is still open even on the Wii).
4. Verification: the GPU output must match the software rasterizer. Plan: a probe app that renders the same recorded
   primitive list both ways and dumps images to the SD card for a pixel diff on the PC.

That is a multi-stage project. Each stage can only be checked in Cemu or on a Wii U by the owner.

## Why it is not the first thing to do: the CPU side is far too slow

New metric in the Wii U app (`vb/s=` in the status line and `session.log`): guest VBlanks per wall second. The core
counts NTSC frames, so 59.94 vb/s is 100% of a real PS2. Measured on the x86 host with the owner's BIOS (interpreter,
Fastmem cache on): about 2 to 3 vb/s while the BIOS boots, i.e. **3 to 5 % of real time**. In Cemu the EE interpreter
reaches about 1.2 M instructions per second against 295 M cycles per second on a real EE. The GS share of the time is
about 10 % of the interpreter in the BIOS boot animation (gprof, host), so moving all GS work to the GPU would not move
the total much until the EE and IOP are much faster. The EE JIT (`make JIT=1`) was slower than the interpreter in Cemu
because Cemu translates the generated code a second time; its value on real hardware is unmeasured.

## Order of work that follows from this

1. Measure real games with `session.log` (item 4): speed in percent, where the time goes.
2. CPU first: EE/IOP execution speed, using the Wii U's three cores where the architecture allows it (for example
   IOP on its own core), measured on hardware, not only in Cemu.
3. Then the GS on the GPU, in the stages above, once the CPU side stops being the bottleneck.

## Measured cost per emulated instruction (x86 host, callgrind, owner's BIOS, 2026-11-23)

`-O1`, interpreter, Fastmem on, BIOS start (1.92 M EE instructions incl. the 1/8 IOP share), start-up cost subtracted:

| Part | Host instructions per EE instruction |
|---|---|
| Whole emulator | about 535 (2.14 G for 4.0 M EE instructions) |
| `ee_step_budget` (self) | about 227 |
| `ee_retire_instruction` (self) | about 87 |
| IOP (`iop_step`, `iop_prepare`, ...), per IOP instruction | about 640 (the IOP runs 1/8 as many instructions, about 16% of the run) |

A tuned interpreter needs roughly 20 to 40 host instructions per guest instruction and a recompiler roughly 5 to 10 (typical
figures, not measured here). Part of the 227 is old per-game investigation code (R1236, R1239, R1245, R1249 ...) that runs
for every instruction; `GEKKO2_FAST_DIAGNOSTICS` removes only about 3% of the total, so the cost is spread over many
small per-instruction checks (option lookups, timer and interrupt polling). This headroom is in the software, not in the
hardware. It is a measurement on a cloud x86 CPU, not on a Wii U.
