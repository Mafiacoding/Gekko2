# Gekko2 R1370 beta — GX triangles and ARM boot RAM

## Changes

- New resident GX hybrid path for textured triangles (including Gouraud), using exact existing CPU STQ/UV interpolation, texture filtering and TFX shading. GX receives the shaded snapshot and performs supported RGB operations/fixed-coefficient blending into its resident framebuffer. Constant-Z GS tests and Z writes remain an exact CPU shadow. It does not move VU execution or all shading to GX.
- Covered-fragment alpha analysis ignores padding and depth-rejected fragments. This allows a constant source-alpha draw to use the existing exact blend program even when unused snapshot pixels have another alpha value.
- Unsupported alpha tests, fog, dithering/FBA, framebuffer masks, mipmapping, varying Z, tiny draws, variable source-alpha blend coefficients and unsupported target/depth layouts retain software fallback. Texture and VRAM updates are preserved.
- Scalar triangle coverage advances integer edge functions with additions instead of recomputing three edge products for every pixel. Barycentric and final color arithmetic retain their existing order.
- ARM capability bit 1 enables a new bulk shared-RAM fill command. It performs 64-KiB to 1-MiB jobs with aligned word stores, validated spans, disjoint descriptors/header, cache publication and byte-for-byte PPC verification. Private module/IOS memory regions are excluded.
- EE cold-boot RAM initialization calibrates one real 64-KiB ARM fill against the PPC clear including cache publication. Remaining initialization uses ARM only if the complete verified ARM path is faster; otherwise PPC handles it. This affects initial host RAM clearing, not the BIOS guest's ongoing EE instruction stream. It is not an ARM EE JIT, SPU2 worker or VU worker.
- Single `build_version.h` supplies the launcher, BUILD lines, session identity and new log filenames.

## Installation / test

Extract Gekko2-R1370-Beta.zip onto the SD root. It contains:

- `apps/gekko2/boot.dol`
- `pcsx2/arm/Gekko2-ARM-Worker.elf`
- this change/test guide

Use your already working ARM/IOS222 setup. Restart the Wii before testing so the previous resident ARM module is replaced by the new one. Enable GX drawing plus `GX Gouraud triangles`, `GX resident pipeline`, ARM worker, ARM CPU/RAM assistance and IOS222; keep the other R1369 test settings. Then choose Cold Boot.

Keep the working R1369 available for comparison. Use the same BIOS, settings and phase. Record both image correctness and sustained guest VBlank FPS. `presents_mHz` measures host output updates, not guest FPS.

Log evidence:

- `BUILD checkpoint=R1370-B ...`
- `GX_SHADED_TRIANGLES attempts/accepted/bbox_pixels`; `GS_GOURAUD gx/software/gx_hybrid`
- `ARM_WORKER_CAPS`: raw capability mask now `00000003`
- `ARM_EE_BOOT verified/selected/PPC_ticks/ARM_ticks`: verified 1 means the real probe matched; selected 0 means the ARM path was slower and was deliberately not used for the remaining clear.
- `ARM_RAM completed/bytes`: includes the real probe and any selected boot fills. `ARM_WORKER submitted` still counts CSC video jobs separately; BIOS can legitimately produce zero CSC jobs.

## Validation

- 40 native regressions passed, including 19,584 pixel comparisons between shaded-source and scalar triangle results: STQ/UV, flat/Gouraud, four TFX modes, TCC, nearest/bilinear and padding/coverage.
- Actual linked PPC hybrid path submits GX commands and publishes exact alpha/depth metadata, including a source-alpha draw whose padding differs. GPU commands and matrix setup were mocked; EFB pixels were not validated.
- Existing six source-pack/cache-publication and fresh-scanout checks passed.
- Actual compiled big-endian ARM ELF passed CSC 1/3/8-block RGB32, RGB16/dither and resource-manager tests. Six full 64-KiB/1-MiB patterned fills passed, with preserved guards; seven invalid/misaligned/private/overlap requests rejected before writes. IOS cache/kernel syscalls were mocked.
- Actual linked PPC ARM client validates capability/vector layout and full results, and rejects corrupted output, error responses and misalignment.
- New worker ELF passed the existing loader/region/stack metadata validator. DOL sections match ELF bytes; Wii startup signature and MEM1 bounds validated.

Physical Wii boot, GX image correctness, ARM latency and FPS gains require the Wii test. No 2x speed-up is claimed. Continuous EE offloading to Starlet remains unimplemented.

## Build / references

Use the same recovered devkitPPC GCC 16.1.0 / libogc 3.1.0 SDK as R1369; new ARM worker uses bundled devkitARM GCC 15.2.0, ARM926 big-endian soft-float. `tools/build_r1370.sh` and `tools/build_arm_r1370.sh` reproduce the binaries.

Existing GS shader design and reference kernels remain GPL-compatible. Primary reference interfaces inspected: https://github.com/devkitPro/libogc/blob/master/gc/ogc/gx.h and https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS/Renderers/HW/GSRendererHW.cpp . No ARM64 renderer or JIT was transplanted.
