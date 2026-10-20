# Crash E3: local reproduction and VU timing correction

20 October 2026. Branch `codex/crash-e3-local-repro`, runtime source
`e7aae5872045c9ad8a554f3a2bbead04e398239f`, comparison `f06bd78`.
GitHub main remains `999df60` (runtime BETA12.20); this batch is not merged.
The Wii build retains BETA12.33 and carries the full source commit in SESSION
telemetry. Do not identify it from the BETA number alone.

## Reproduction and actual progression

The owner's three split ZIP volumes reconstruct and validate by size, DEFLATE
end and CRC. The private disc, BIOS, ELF, memory, microcode, snapshots and images
remain outside Git and all source/SD packages. No proprietary test fixture.

Two distinct failures must not be conflated:

1. **Long initial black screen:** unchanged code does eventually enable scanout
   at EE retirement 927,969,999 / IOP 115,993,042. A startup loop computes 65,536
   sine-table entries using software floating point; sampled completed entries
   agree with the expected sine values. The owner's reviewed early logs stop
   before this point. This is real, extremely slow progress, not proof of a GX
   hang. This batch does not accelerate or skip that initialization.
2. **Later geometry/transport failure:** unchanged code reaches logos but emits
   corrupt VU output and eventually stalls at GIF 75,908 / VU 111,374,719.
   PATH1 remains owned, XGKICK pending, a PACKED stream is interpreted as an
   A+D SIGNAL, and VIF1 DMA cannot finish. CLIP-only correction reproduces the
   same terminal counts. Correcting VI branch bypass removes this observed
   failure without clearing SIGNAL, forcing DMA completion or changing guest PC.

Matched resumed baseline/fixed runs use the same pre-VU checkpoint and mask
`00787dff`, 190 additional million IOP slots. Baseline ends at the counts above;
fixed ends at GIF 12,317,657 / VU 107,711,522 with no pending kick and continued
3D title animation. Different VU instruction totals are not a throughput claim.

A separate **cold boot of the final runtime**, without resume or guest patches,
runs 280 million IOP slots. Final EE retirement 2,239,965,094, GIF 22,066,227,
VU 184,163,612; neither CPU halted. VU unknown instructions 0, no pending kick,
GIF owner/pending 0, VIF stalls/unsupported commands 0 and VIF1 DMA QWC 0.
The first enabled scanout is still at the same retirement count as baseline.

Further local runs with ordinary virtual pad input reach **PRESS START →
INITIALISING/LOADING → an in-game corridor with Crash and the Wumpa/lives HUD**.
An initial START press during the earlier intro did not enter the game; a later
press at the actual prompt did. This is an observed scene transition, not a
claim that all levels or sustained gameplay are correct.

Reference PCSX2 **v2.8.2**, software renderer, independently reaches the 3D title
with the same private disc. Its fastboot and this emulator's cold boot are not
matched timing conditions. Captures here are raw 640×224 GS images; comparing
those directly with a scaled 640×480 reference cannot establish an aspect or
pixel defect. Earlier dark/flattened title snapshots alone are not a proven
remaining color/geometry bug. Exact pixel equivalence remains unverified.

## General hardware fixes

- Timed VU0/VU1 conditional branches read the previous VI value immediately
  after integer ALU, MTIR and auto-update LQ/SQ writes. Preserve the earliest
  value across consecutive writes to the same VI, expire on real issue/stall
  clocks, restore canonical VI after branch evaluation, and clear history on
  stop. JR/JALR, flag writes and ILW retain their existing separate semantics.
- CLIP/FCSET retire after four issue clocks. Consecutive CLIP history accumulates
  at issue; lower flag queries see only retired results. Preserve same-pair
  upper CLIP priority, I-immediate handling, FDIV and MAC/STATUS independence,
  FSSET snapshots, operand stalls and E-bit drain.
- These rules surround the shared timed interpreter/AOT/JIT callback. No
  game-name dispatch, BIOS replacement, sine HLE, synthetic packets, fake IRQ,
  unchecked dirty-register chain, save/restore removal or GX fence removal.
  The legacy untimed embedding API intentionally retains immediate semantics.
- New flag payload and VI history use previously unused structure padding.
  Host and PPC layout checks retain the 24-byte queue slots and scoreboard
  offset 104. Legacy snapshots have no VI-history marker; new actual checkpoint
  round trips preserve both units' pending flags/history and later EE/VU fields.
  Use this runtime to resume its new snapshots; old runtimes do not implement
  these in-flight semantics.

Reference semantics reviewed in PCSX2 v2.8.2 `VUops.cpp` (`_vuBackupVI`, six
conditional branches, CLIP and FMAC publication) and the VU micro interpreters.
Implementation and fixtures are synthetic; no PCSX2 program/assets transplanted.

## Validation

- `python3 tools/test_alpha_compat_host.py --output-json <outside-repo>/host.json`
  → **PASS 92 host gates**. Includes new CLIP/FCSET/VI timing vectors, actual
  checkpoint round trip, both outcomes of six branch conditions, delayed It,
  consecutive/different VI writes, actual operand stalls, JR exclusion and
  supported writer classes. Both new tests fail with baseline `vu.c`/FMAC code.
- `python3 tools/test_vu_masks_aot.py` → **PASS 1024 VU0/VU1 AOT/pipeline
  comparisons** using generated synthetic fixtures. A direct standalone build
  of `test_vu_aot.c` requires that generated fixture; use this driver.
- Actual linked Wii PPC checks cover VU timing and VI history across native
  blocks, flag/query ordering, RAW/Q, budgets, source invalidation, ABI and both
  production runners. Platform allocation/cache services are mocked; no GX/VI
  hardware or cycle measurement. Full ELF-specific records accompany the review
  artifact. The obsolete `verify_vu_pipeline_internal.py` references removed
  `vu_jit_try_block` and cannot run on this runtime; the current timing/hybrid
  verifiers exercise the production entry points instead.
- Both fresh Wii builds use current official devkitPro/devkitPPC installation:
  tested **devkitPPC r50 / GCC 16.1.0 / libogc 3.1.0**. Preserve SPEED, AOT, DSI,
  native GX, resident-fence and existing experimental compile flags. The control
  build omits EE chain residency; the companion compiles it, runtime default OFF.

```sh
export DEVKITPRO=/workspace/.devkitpro
export DEVKITPPC=$DEVKITPRO/devkitPPC
export PATH=/workspace/toolchain-helpers:$DEVKITPPC/bin:$PATH
CHAIN_RESIDENCY=0 bash tools/build_beta12_33_windows.sh
CHAIN_RESIDENCY=1 bash tools/build_beta12_33_windows.sh
# Linked verifiers accept positional ELF followed by --nm <powerpc-eabi-nm>.
```

Both DOL/ELF load-section comparisons and static MEM1 gates pass:
control static end `0x815c6474`, remaining **2,333,580 bytes**;
chain static end `0x815e7a54`, remaining **2,196,908 bytes**.
Static headroom is not proof of runtime heap capacity.
No physical Wii or Dolphin execution of this batch. Existing Lightrec compiler
warnings remain; they are not suppressed by a code change here.

Host disc reproduction uses `tools/build_host_core.py game_check_r1386` with
`HOSTEXTRA='-DGEKKO2_IDLE_SKIP -DGEKKO2_SPEED_STUB -DGEKKO2_SPEED_RETIRE -DGEKKO2_GAME_TRACE'`
and `GEKKO2_OPTIONS=00787dff`. Host builds do not execute emitted PPC; the
separate linked checks do. Full cold-run command shape:
`GEKKO2_CAPTURE=<private-dir> <game_check_r1386> <owner-bios> <owner-disc> 280 <private-checkpoint>`.
Inputs, captures and raw output must remain private.

## Remaining acceptance

- Real-Wii GX Hybrid progression, physical input and sustained stability/FPS.
  Host software GS reaching a level cannot establish GX pixel correctness.
- Slow software-float sine initialization remains. Profile the actual native
  EE path before claiming a JIT speed fix; do not substitute a game-specific
  precomputed table or invent host-to-Wii FPS equivalence.
- Broader Crash levels, Tekken and GT2000/GT3 regression with these timing rules.
- BIOS tile discrepancy, the separate rare owner BIOS GX timeout, missing
  hardware semantics and general PS2 compatibility remain open.
- No public alpha or main merge solely on these local results. Deliver a review
  candidate with the retained fallback and exact build identity.
