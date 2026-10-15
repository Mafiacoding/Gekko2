# Instructions for Codex and coding agents — Gekko2

**Read `docs/CODEX_HANDOFF.md` before changing code.** Read the relevant `docs/BETA12.*` reports and the newest `STATUS.md`. The owner communicates in German; use German for user-facing status unless requested otherwise.

## Scope / authoritative baseline
- This is an experimental **PS2 emulator for the original Nintendo Wii** (Homebrew Channel / Dolphin test platform), using devkitPPC, libogc, a PPC EE/IOP dynarec/AOT, VU interpreter/AOT, GS/GIF/VIF hardware simulation and Wii GX.
- GitHub `main` was synchronized to **R1389-SPEED-BETA12.20-GX-ALPHA-FENCES** on 2026-10-15. The release string in `include/build_version.h` is authoritative. Recheck `main` before starting; do not silently overwrite later user edits.
- The primary near-term objective is **real game progression and correct graphics for Crash Bandicoot: The Wrath of Cortex E3 demo**, then Tekken Tag Tournament, Gran Turismo 3, BIOS/OSDSYS and King of Fighters, while preserving proven performance.
- These are experimental partial implementations, **not** proof that the PS2 hardware or dynarec is complete; no verified gameplay fix or faster Wii FPS in BETA12.20.

## Work rules
1. Inspect current code and tests before proposing changes. Prefer small reproducible batches and follow existing emulator contracts; explain exactly what changed and what remains open.
2. **Do not commit or upload any PS2 BIOS, game disc, ROM, user log, proprietary AOT trace/profile, memory card, keys or dumps.** Do not include private assets in logs, CI fixtures, source ZIPs or public issues. Use synthetic, legally distributable test inputs. Respect upstream PCSX2/Play! licenses: implement from documented behavior, do not blindly transplant code.
3. Keep an interpreter/software-GS oracle and fallback. Differential-test native JIT/GX changes. Never invent missing assets, synthesize screen tiles, switch to the backbuffer as a cosmetic black-screen fix, skip IRQs, fake cycle counts or report host tests as Wii hardware evidence.
4. All code-cache/direct-link exits must validate **code generation, mapping/TLB/ASID, memory writes including DMA/self-modifying code, scheduler budget, pending interrupts and exceptions**. Flush dirty guest registers on invalid transitions. Preserve architectural EE state and delay-slot exception EPC/BD.
5. For PPC 128-bit residency, obey PPC ABI and `r18..r29` save/restore contracts. The existing allocation applies **inside** a native block only; cross-block retention is NOT implemented. Never simply remove prologue/epilogue saves, return through `bctrl` with uncommitted state or bypass helper/exception barriers.
6. For GS/GX, preserve physical 4 MiB VRAM wrap, PSM swizzle and aliasing, FBA/FBMSK/DATE/ATE/AFAIL/depth semantics, GX readback failures, CPU/GPU cache fences and safe fallbacks. Native GX is not automatically pixel-equivalent to PS2 GS.
7. Maintain Wii memory limits (MEM1/MEM2) and avoid host-only APIs or an accidental legacy toolchain. Preserve all existing SPEED/DSI/AOT/VBlank flags unless deliberately running an isolated comparison.
8. Build in a fresh build directory or remove stale object/dependency files when changing version/configuration: stale `.o` files have previously retained old release strings and compiler flags.
9. Report **static checks, host simulation, Dolphin testing and real-Wii tests separately**. Never claim Crash/Tekken/GT3 is playable without evidence from an actual run.
10. Do not merge unverified high-risk changes into `main` without a clear test record. Prefer a working branch and reviewable PR for large JIT/GX/VU modifications; keep a rollback DOL.
11. Codex cannot access the user's physical Wii. Hardware-dependent conclusions require a fresh `SESSION checkpoint=...` log or owner-provided screenshots/measurements.

## Entry points
- `docs/CODEX_HANDOFF.md` — complete current context, prioritized work batches, tests and success criteria.
- `source/core/recompiler/ppc_dynarec.c`, `ppc_residency_internal.h`, `ee_jit.c`, `iop_jit.c` — CPU/JIT.
- `source/core/ee/ee_core.c`, `source/core/iop/iop_core.c`, `source/core/vu/vu.c`, `source/hw/vu.c` — execution/timing.
- `source/hw/gif.c`, `gs_gx.c`, `gs_mem.c`, `gs_wii_output.c`, `vif.c`, `dma.c` — GS/GIF/GX/DMA.
- `source/hw/ipu.c`, `ipu_mpeg.c`, `ipu_vlc.c`, `ipu_idct.c` — video/IPU.
- `tools/build_beta12_19_fb16_jit.sh`, `tools/verify_dol_r1352.py`, `tools/verify_wii_memory.py` — baseline build/verification (adapt target/version to new releases).

## Delivery for each batch
Provide a concise German explanation, touched paths, the original failure, regression tests and their **actual** output, toolchain/flags, build status, Wii DOL + source/patch if successfully created, performance evidence where available, known remaining gaps and a request for a real Wii log when needed.
