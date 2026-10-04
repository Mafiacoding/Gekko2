# PCSX2-Wii R1274: EE dispatch overhead and field presentation

R1274 retains VU0/VU1 native blocks and changes one measured EE dispatch decision plus the Wii presentation scaler. The full EE/IOP/VU JIT remains incomplete. R1273 is saved as fallback.

## What was found

The authentic OSDSYS checkpoint programs a 640x256 GS field (SMODE2=3). The old Wii output copies nearest source rows into 640x480 (and development captures into 640x448), producing visibly repeated/jagged vertical edges. This is a presentation problem in addition to the PS2 menu's inherently low resolution; it does not prove all remaining texture/font issues are fixed. Mixed/mipmapped GS texture filtering remains partial.

An actual-PPC comparison of eight ADDIU instructions including the full EE epilogue finds:

| Build/path | Cold PPC instructions | Warm PPC instructions |
| --- | ---: | ---: |
| R1273 Interpreter | 5,217 | 5,217 |
| R1273 single-op JIT | 5,953 | 5,513 |
| R1274 Interpreter | 5,073 | 5,073 |
| R1274 cost-aware JIT build | 5,073 | 5,073 |

Single-instruction JIT dispatch/lookup/call costs exceed ADDIU's tiny arithmetic body: old warm +5.674%, cold +14.108% versus interpreter. R1274 executes ADDIU through the existing in-function interpreter case in the CPU loop. Its native compiler and direct JIT API remain available, and other opcodes plus VU blocks are unchanged. This is deliberately a hybrid dispatch choice, not completion of EE block JIT. Counts and normal guest retirement remain unchanged. The measured ADDIU loop uses 7.98% fewer PPC instructions than the old warm JIT path.

Conversely, R1273 VU1 JIT versus R1273 Interpreter uses 27.80% fewer PPC instructions in the branch test, 65.29% fewer in the straight test, and 58.55% fewer in the mixed test, with identical final state. The user's whole-emulator/hardware slowdown cannot be attributed to VU arithmetic from these measurements. Compilation startup, other EE/IOP paths and hardware cache/memory costs require further profiling; no hardware explanation is claimed as proven.

## Presentation fix and measurement

The scaler caches two converted source lines in a bounded 4KB stack cache and interpolates Y/Cb/Cr at centered vertical sample coordinates. Source GS RAM and textures are untouched; horizontal sampling remains the prior nearest convention. 1:1 scaling stays exact, edge rows clamp, odd widths decline and outputs wider than 1024 retain the old nearest path. This is vertical presentation filtering, not a higher-resolution GS renderer or full weave deinterlacing.

Actual Wii ELF 64x24 -> 64x48 presentation fixture, three identical samples: R1273 332,276 PPC instructions; R1274 229,342 (30.98% fewer), correct filtered colors and preserved output bounds. Saving source reads/conversions compensates interpolation work in this test. No Wii frame-time/FPS claim follows.

Before/After images use the exact same authentic BIOS checkpoint, the old/new C presentation functions and the same YCbCr-to-RGB preview conversion, output 640x480. They are development captures, not screenshots from physical Wii. Displayed color conversion is identical between them.

## Verification/reproduce

180 native regressions pass, including updated centered row interpolation, source offsets, output bounds, identity, downscale and odd-width rejection. Both actual ELF variants pass inherited EE/IOP/VU0/VU1/pair/block/VIF0 tests. Actual PPC scaler tests verify pixels, bounds and instruction counts.

Build tools/build_r1274.sh with supplied devkitPPC/libogc and -j1. measure_ee_frontend_r1274.py and measure_gs_scaler_r1274.py take ELF and --nm. compare_gs_scaling_r1274.c captures old/new presentation from a private BIOS checkpoint without advancing guest state. diagnosis_r1274.json holds the measured records.

Open: EE/IOP blocks/event-boundary correctness, broader cost profiling, block linking/register allocation, PS2 VU pipeline/float/flags/Q/P/EFU accuracy, mixed/mipmap GS filtering and real Wii FPS. No new Tekken result is claimed. Public checkpoint excludes BIOS/game/raw RAM; cumulative patch/diff apply against original pcsx2-wii-full-debug(1).zip with exact verification.

## Authentic BIOS continuation

Cross opens Browser, release, Circle returns to OSDSYS, release. Native interpreter EE 1,920,964,493 to 2,041,839,119: 120,874,626 more instructions, halt=0; no guest RAM/PC/menu flags forced. This is checkpoint continuation, not a new cold boot or a generated-PPC BIOS run. General menu bitmap pixels remain PS2 low-resolution; the new filter only improves vertical presentation.

PPC fixtures mock allocation and code-cache maintenance primitives. Cold counts exclude their real runtime/hardware costs. All instruction counts remain synthetic work measurements rather than cycle or FPS claims.
