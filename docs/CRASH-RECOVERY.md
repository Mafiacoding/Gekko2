# Crash recovery and stability integration (BETA12.20 base)

Scope: verify earlier Crash fixes against the code, port only what the
ChatGPT snapshot (SPEED BETA12.33) proves missing via reproducible tests.
No dynarec/scheduler/audio/memory-card changes were adopted.

## Phase 1 – verified present in code (with passing tests)
BETA12.2 JIT executable RAM 0x00100000–0x001FFFFF; 12.3 FILEIO 2050/2110,
old/new OPEN structs, IOP module version; 12.6 FILEIO path parser/aligned
offset 8; 12.18–12.20 GS/GX 16-bit FB, ATE, DATE, FBA, FBMSK, Z16/Z24,
format conversion (see GRAPHICS-FIXES-INTEGRATION.md).

## Newly integrated
* JIT floor: chain-entry `backed_candidate` still used 0x200000; now
  0x100000, matching `ee_core_block_pointer`.
* Unimplemented EE opcodes raise a precise Reserved-Instruction exception
  (ExcCode 10, EPC/BD, both vectors) via `ee_core_raise_reserved` instead of
  halting the emulator; COP1 C.F.S implemented.
* EE CPU FIFO backpressure (LQ/SQ to VIF0/VIF1/IPU parks the instruction).
* CDVD GetError SCMD RPC.
* Masked-level interrupt window admission (Cause IP2/IP3 exact, transient
  INTC latching stays precise).
* IOP ELF relocations: R_MIPS_NONE/16/LO16 standalone, SCE MHI16+ADDEND with
  chain validation; malformed/cyclic chains rejected.
* VU (CLIP latency, VI branch bypass, ADD alignment, ILW/ISW masks), VIF
  (DIRECT streaming, micro barriers, CPU FIFO), DMA (IPU stall, SPR
  interleave), IPU (IDEC fields, soft-reset predictors, coefficients).
* 24 snapshot regression tests added under tests/.

## Test results (host)
373 tests total. 48 fail in the generic harness; the 44 that also exist at
the previous commit fail identically there (link recipe/stub issues) — no
regression. Of the 4 new ones, `test_ipu_coefficients` (12201 cases) and
`test_ee_cdvd_error_rpc` (2912 checks) pass with manual link exclusions
(nested self-includes the generic recipe cannot see). `test_spu2_envx` and
`test_completion_spu2` need SPU2 changes that were deliberately not ported
(audio). Newly passing: completion_ee_control, ee_masked_window (1557),
iop_reloc_variants (774), gs_psm_convert_beta12_11.

## Not done / open
* Gekko2 cannot run the Crash demo without a PS2 BIOS (not available here);
  no in-game Crash run was performed in Gekko2. PS2Recomp+Dolphin runs are
  separate (see handoff docs).
* GS narrow-MMIO, memory-card and mcserv hunks of the snapshot not ported.
* SPU2 envelope/noise/volume-counter changes not ported.
* No physical-Wii verification of any of this.

## Crash E3 original fix audit (branch codex/crash-e3-local-repro, e7aae58)
Source of truth: commit e7aae5872045c9ad8a554f3a2bbead04e398239f
("Fix VU conditional branch bypass and delayed CLIP flags"), report
docs/CRASH-E3-VU-TIMING.md (commit 51e9051/24b69fe), copied verbatim.

Byte comparison of this repo against branch tip 24b69fe:
identical: include/core/hw/vu_timing.h, source/hw/vu.c,
source/hw/vu_fmac_internal.h, tests/test_vu_vi_branch_latency.c,
tests/test_vu_clip_latency.c, tools/verify_vu_timing_r1384.py,
tools/verify_vu_hybrid_r1385.py (this commit), and gif.c, gs_gx.c,
gs_gx_surface.c, gs_mem.c, gs_wii_output.c, vif.c, dma.c.
Before commit e585867 of this branch main (5d3b3d3) did NOT contain the VU
fix; it has been present since e585867.
Only difference: source/hw/gs.c (GS IMR unmask delivery, lane-aware narrow
CSR/MMIO, game_trace hooks). Not ported: it needs the EE narrow-MMIO hunks and
is a general GS feature, outside the VU/graphics scope requested.

Results (original branch tip and this tree behave identically):
* test_vu_vi_branch_latency, test_vu_clip_latency: PASS in both trees.
* Linked actual Wii ELF (release flags, built from this tree):
  verify_vu_timing_r1384.py PASS (delayed CLIP/history/FCSET, six VI branch
  conditions, bypass expiry); verify_vu_hybrid_r1385.py PASS (VI history
  across native blocks, delayed CLIP callback, production VU0/VU1).
* verify_dol_r1352 PASS, MEM1 2,855,820 B free (>= 1 MiB).
* Harness-only failures (test_dma_gif_demo, gs_blk_addressing,
  gs_local_host*, gx_flat_r1290/94, checkpoint_pad, link errors) fail
  identically on the original branch tip with the same recipe; they are not
  caused by this tree. test_vu_aot/test_ee_aot/test_iop_aot need a generated
  fixture header that is not in the repository.

## Crash acceptance sequence — NOT performed
Cold boot through sustained gameplay requires the owner's PS2 BIOS and disc
image, which are not in this environment. Nothing in that sequence (black
screen sine-table init, scanout, VU geometry, XGKICK/PATH1, VIF1 DMA, title,
PRESS START, loading, corridor/HUD, stability) was run here, on host or Wii.
The original report's own limits stand: no physical Wii or Dolphin execution
of this batch; real-Wii progression and stability remain open.
