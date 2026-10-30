# Crash graphics fixes: audit and integration (2026-10-30)

Compared: this repository (`main` = BETA12.20, commit `5d3b3d3`) against the Gekko2 snapshot delivered with the Crash GX handoff (BETA12.33 line, developed on the side branches `codex/beta12-21 ... 12.33`; `main` was never updated). Only GS/GX files were taken from the snapshot; EE scheduler, dynarec, JIT, VU/IOP/SPU2 and all performance code were left exactly as in `main`.

## 1. Fixes taken over (verbatim snapshot implementations, not re-implementations)

Audit result per requested item (what `main` already had, what was missing):

| # | Item | `main` (BETA12.20) | Taken over |
|---|------|--------------------|------------|
| 1 | TRXDIR=2 `Move()` | `gs_mem_copy_local` incl. overlap direction, same-PSM and the BETA12.11/12.13 conversion + alias paths already present and identical in the snapshot | no code change needed; test `test_gs_psm_convert_beta12_11` was stale (expected the pre-12.13 alias reject) and is replaced by the snapshot's updated version |
| 2 | DATE | present for CT32/CT24 | snapshot refinements for 16-bit/GX admission (see 7) |
| 3 | ATE / AFAIL | CT32 only; AFAIL=RGB_ONLY alpha preservation wrong for CT16/16S and ordering vs FBA/clamp wrong | `gs_finish_pixel` rewritten as in the snapshot: RGB_ONLY keeps old alpha also on 16-bit targets (`gs_framebuffer_write16(...,preserve_alpha)`), unclamped blend result, one clamp after dither |
| 4 | FBA | applied after the AFAIL special case | FBA now corrects the source alpha before the write masks, AFAIL=RGB_ONLY still forbids an alpha write |
| 5 | FBMSK | per-halfword mask for CT16/16S only partly honoured with AFAIL | taken over with the same `write16` path |
| 6 | PSMCT24 | CT24 alpha byte preserved | taken over: CT16 framebuffer channels expand with zero low bits (`b<<19|g<<11|r<<3`) like the GS blend, not 5-to-8-bit replication; Z textures (PSMZ32/Z24/Z16/Z16S) sampled through the Z swizzles with TEXA/AEM (`gs_is_depth_texture`) |
| 7 | CPU fallback for complex blending | GX admission treated every `DTHE` as dither | `gs_dither_active()` (dither only on CT16/CT16S) in all GX/native admission gates; CT16/CT16S frames admitted to the GX sprite/triangle/untextured paths; unsupported blend/alpha/depth states still return to the exact CPU path; GX import (`gs_gx.c`) takes the snapshot's DATE/ATE/AFAIL/FBA/FBMSK action order, 16-bit targets and the extended native-triangle path (compiled only with `GEKKO2_GX_EXTENDED_TRIANGLES`) |

Also in the snapshot GS files and therefore included: exact 10.4 UV phase (`double`), Q/K/L per-fragment LOD and MTBA sampling, physical CLUT halfwords (CSM/CPSM reinterpretation), `gs_gx_pack_rgba8_psm` / `gs_gx_present_psm`, protected-resolver texture digest, CT16 GX presentation (`frontend_allow_gx_output`).

## 2. Changed files

Modified: `include/core/hw/{gif,gs_gx,gs_gx_surface,gs_wii_output}.h`, `include/core/runtime_profile.h`, `source/core/checkpoint.c` (one line: accepted GIF state sizes), `source/core/runtime_profile.c`, `source/hw/{gif,gs_gx,gs_gx_surface,gs_mem,gs_wii_output,frontend_runtime}.c`.
New (tiny dependencies of the snapshot GS code): `include/core/hw/game_trace.h`, `source/hw/game_trace.c` (compiles to nothing unless `GEKKO2_GAME_TRACE`), `include/core/recompiler/experimental_options.h`, `source/core/recompiler/experimental_options.c` (all options default OFF), profiler work counters in `runtime_profile.*`.
Tests: 27 snapshot GS/GX tests added or updated (oracle tests for AFAIL/FBA masks, blend/dither formats, depth textures, DATE, GX fragment actions, PSM modes, CLUT, scanout), `tests/test_checkpoint_sprite_r1297.c` adapted to the grown GIF state layout.

Deliberately NOT taken: `source/hw/gs.c`/`gs.h` (BETA12.30 IMR/CSR pending-event semantics, tied to EE memory-access changes outside this scope; `test_gs_registers` still passes), EE/JIT/dynarec/scheduler, memory card, settings/menus, SPU2/IOP/IPU/VIF changes, network.

## 3. Tests

Host (gcc, `tests/run_test.sh` recipe with a CHD stub; 352 test programs):
- Before (HEAD): the new oracle tests fail or do not build against `main`: AFAIL/FBA write-mask oracle 32 of 4096 cases fail, blend/dither oracle 7514 of 32768 fail, depth-texture assertion fails, `gs_dither_active` missing.
- After: AFAIL/FBA oracle 4096/0 failures, blend/dither 32768/0, depth textures 6144 cases, GX fragment oracle 81920 cases, `test_beta12_20_gx_alpha` 159 checks, DATE (r1378), FBA, frame mask, PSM modes 10816 cases, `test_gs_psm_convert_beta12_11`, local-move: all pass.
- Whole suite: 309 pass, 43 fail. All 43 failures are identical on unmodified `main` in this harness (tests that need their own link recipe/stubs or are skipped by feature flags; a few are real `main` failures such as `test_gx_flat_r1290`). One genuine regression found by the full run and fixed: GIF checkpoint layout (see below). The 43 were not investigated one by one.

Wii build (devkitPPC 16.1.0, libogc, `make` default): builds. Release-flag build (`tools/build_beta12_19_fb16_jit.sh` flags, without `LIGHTREC_PILOT` because `lightning.h` is not generated in this tree): `main` and integrated both build and pass `verify_dol_r1352.py` and `verify_wii_memory.py`. Static MEM1 remaining: 3,190,572 B (`main`) vs 2,861,036 B (integrated), minimum 1,048,576 B.

## 4. Behaviour changes and remaining issues

- GIF state layout changed (UV as `double`, new fields): checkpoints written by BETA12.20 builds (3832-byte GIF block) are rejected cleanly on load instead of being misread; save states must be recreated. The snapshot lineage layouts (and the pre-sprite-coordinate legacy layout) load.
- `sizeof` and MEM1 use grew by about 330 KB static; headroom still above the 1 MiB rule.
- Crash demo: not run in Gekko2 itself because it needs a PS2 BIOS (none available here). The same snapshot GS/GX sources are exercised by the PS2Recomp hybrid, which boots Crash in Dolphin with deterministic frame hashes; that is not a Gekko2 run and not physical Wii.
- Still open (unchanged from the snapshot's own lists): GX for all game blend states (resident EFB shader/TEV), 16-bit DATE/FBMSK on GX, AA1 coverage, framebuffer feedback loops, dither/texture shuffles, TRXDIR=1/VRAM address wrap/all physical alias cases, mixed MMAG/MMIN GX filtering, startup fade is long (dark early frames are a legal subtractive fade, not a rendering fault).
