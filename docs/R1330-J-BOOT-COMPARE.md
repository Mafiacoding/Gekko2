# Gekko2 R1330-J — boot comparison and scheduler corrections

The owner reports that R1330-I loads in Dolphin, keeps the quantum-log fix on PC/Wii, but no longer reaches the BIOS picture. The Wii photo shows EE/IOP continuing and GX disabled. The subsequently supplied logs distinguish a GX-enabled run without a crash and a software run with a reported crash. A physical retest of R1330-J remains pending.

The GX log covers about 488 seconds, ends PAUSED at EE=168,582,803 / IOP=21,065,285, and eventually has PMODE=66 / DISPFB2=1400 / source=640x256. Nevertheless FIRST_IMAGE stays zero, all 97 GX reports have attempts=0, and every GX wait/draw counter is zero. The GX backend never reached initialization or the new FIFO fences. This log establishes guest progress and a first-image gate still closed, not a GX FIFO failure or a hard stall at 1.2 million instructions.

The software log covers about 35 seconds and ends at EE PC=8000e5f4 / IOP PC=00016b04 with no pause/stop/crash register dump. It cannot identify the reported host crash instruction. Its initial EE_BLOCK retirement and GS_ROUTE counters exceed work possible in that boot, so those process-lifetime counters include a previous run; they must not be interpreted as current-session draw activity.

## Reproduced scheduler defects

1. `ee_core_step_n` bounded its loop by actual retirements. An idle EE advances hardware/park time without retiring instructions, so an eight-instruction grant could fail to return and starve the IOP. The unchanged R1330-I native host regression timed out; the linked PPC regression exhausted its instruction budget in `system_run_interleaved`. R1330-J bounds the grant by scheduler slots while still returning the actual retirement count. It never increases that counter for idle or scalar fault/redirect attempts.
2. Scalar HLE handlers can return early without setting the CPU halted flag. The R1330-I EE-driven scheduler then returned an incomplete/zero quantum and lost its partial-slot accounting on reentry. A linked PPC syscall-100 program reproduced zero completed quanta without a halt. The new interleaved entry keeps the unfinished quantum and resumes its remaining slots until the IOP boundary, a real halt, or callback termination. Existing HLE state/retirement/timer behavior is retained.

These defects are fixed and tested independently. The continuing EE counter means the pure idle deadlock does not explain that particular run. The logs also show a later throughput fall from roughly 860,000 to 118,000 EE instructions/second before any GX present. No claim is made that the owner's exact slowdown or software-mode host crash is resolved without a physical retest.

3. **First-image false negatives.** The previous 32x16 grid skipped real RGB between its sample points. Independent host tests reproduce misses at (1,1) and the last (639,255) pixel of a 640x256 scanout. The fast grid remains; if it finds nothing, bounded 64-pixel row reads check the complete active region before declaring it black. Alpha-only black and RGB outside the displayed crop still do not unlock the gate. This preserves genuine content detection instead of forcing GX ready. It may add CPU work while the display is configured but remains completely black; after the first confirmed image it is no longer called. The supplied logs do not contain raw VRAM, so they cannot prove that this false negative was the particular cause of the owner's closed gate.

## Two controlled builds

- **Gekko2-R1330J-Boot-Compare.dol**: primary comparison build. Same non-FAST EE frontend and IOP-driven scheduling selection as the supplied R1330-H ELF (scalar instruction JIT remains enabled). It includes the bounded idle correction, R1330-H quantum-log fix, and R1330-I GX ownership fences. It is a recompile using the supplied r32 toolchain, not a byte-identical GCC16 binary.
- **Gekko2-R1330J-Wii-Idle-Fix.dol**: experimental R1330-I FAST/block path plus both scheduler corrections. Dispatcher/fused-boundary/GX improvements remain available for controlled comparison. It is not promoted as a verified BIOS boot build.

The Makefile again defaults to the original non-FAST configuration. `tools/build_r1330j.sh` builds both explicitly. Runtime `BUILD` lines identify whether EE blocks and EE-driven scheduler quanta are active. `CORE_WAIT` records EE idle/halted/Count/retirements and IOP idle/halted/scheduler ticks. `GS_SCANOUT` records current display registers, crop, probe attempts, and draw-buffer selection. New boot resets frontend image/GX attempt counters; existing JIT/route counters remain process-lifetime counters. None of these SD diagnostics adds console quantum spam.

## Test and install

Regression logs and `Verification.json` are included in the checkpoint. Tests cover parked EE grants, actual IOP progress, boundary wake, nonhalting HLE early yield, timer/display timing, native delay/vector/ABI/invalidation paths, real EE/IOP/SIF mailbox transfers and deferred GX readback ownership. These are synthetic host/linked-PPC checks, not a full private BIOS boot or Wii hardware verification.

Use the comparison DOL as `sd:/apps/gekko2/boot.dol`; the checkpoint's `apps/gekko2` directory uses that build. Cold boot with the same BIOS/menu/disc/HUD settings as R1330-H and R1330-I. Do not resume a running emulator to compare execution modes. The experimental FAST DOL is under `experimental/`.

Upload `sd:/pcsx2/Gekko2-R1308-software.log` for GX off, or `sd:/pcsx2/Gekko2-R1308-gx-render.log` for GX on. The old filenames remain intentional. Compare the last BOOT/FIRST_IMAGE, BUILD, HOST, CORE_WAIT, GX and EE_BLOCK lines. `FAULT PC=0` in the HUD describes a last-fault observation; it is not proof that the current EE PC is zero.
