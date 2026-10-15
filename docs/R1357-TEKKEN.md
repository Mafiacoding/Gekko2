# R1357 Tekken Beta

R1356 remains the unchanged control. This build preserves its optimization switches, ARM worker interface and DSI path; this release does not claim new ARM acceleration or measured Wii speed improvements.

## Observed fixes

- **toSPR TTE:** channel 9 chain transfers now copy the full 16-byte DMA tag into scratchpad before its payload, including zero-QWC tags and scratchpad wrap. The VIF upper-64-bit TTE path remains separate. Tekken previously read payload bytes as a packet length, submitted 15,038 quadwords from scratchpad and terminated GIF MFIFO at an unintended zero tag. Reference: PCSX2 `SPR.cpp`, `_SPR1interleave`/chain TTE and `SPR1transfer(ptag, 1)`; https://github.com/PCSX2/pcsx2/blob/master/pcsx2/SPR.cpp .
- **VU IOR:** lower SPECIAL funct 0x35 is IOR; 0x33 is reserved. Both the shared VU0/VU1 micro interpreter and PPC translator are corrected. The observed Tekken word was `0x80020875` at VU1 PC 0x558/0x698. A reproduced section previously accumulated 48,696 missing VU1 operations; the corrected run accumulated none. Reference: cached PCSX2 VUops.cpp LowerOP_OPCODE.
- **XPADMAN:** the 128-byte DMA slots now expose currentTask at byte 114, MODE_CONFIG_READY=2, two actual supported digital/analog modes and the selected mode/axes. Modern SET_MAIN_MODE command 6 uses its real +20 result field and the existing SIO2 mode backend. Unsupported ports/modes do not claim success or create new buffer bindings. Both slots and status frames now advance with actual VBlank rather than instruction count. Actuator/pressure functions remain unsupported. References: ps2sdk ee/rpc/pad/src/libpad.c, iop/input/padman/src/freepad.h and padMiscFuncs.c.
- **Checkpoints:** optional CDRL block preserves legacy CD-ROM registers, FIFOs, pending response and sector cursor/data. It stores no open file pointers; the mounted image must be available for restoration. Older checkpoints still load but cannot reconstruct absent controller state or undo already corrupted scratchpad data.
- **Logs:** TEKKEN_DMA is included in existing snapshots, with GIF drain and fromSPR producer/ring registers. No new per-instruction disk logging.

## Native game validation

A genuine supplied SLUS_200.01 disc image and BIOS were executed. A fresh corrected boot displayed the Namco logo and Tekken title on black. With the XPADMAN corrections, normal START opened the main menu and CROSS entered character selection. The final source, including VBlank-based pad refresh, reproduced the menu at 1,903,928,525 EE instructions and Xiaoyu character selection at 2,103,921,872. The supplied PNGs are exact 640x224 native scanout field captures, converted from PPM; they are not Wii photographs or generated images.

The prior DMA-stuck section, resumed before the failure, continued through 4,503,865,313 EE instructions with new VU activity and 175,287 submitted/drawn triangle accounting versus the earlier frozen 1,630. These counters establish resumed work, not an FPS speedup. Degenerate geometry remains substantial, and 3D character models are not yet correctly visible. Gameplay/full compatibility, physical GX/DSI/ARM behavior and Wii frame rates are not established by these host runs.

## Verification

`python3 tools/verify_r1357_native.py`: all 12 focused native regressions pass: literal VU IOR semantics, actual PPC IOR/IAND emission and reserved rejection, VU micro/pair behavior, modern/legacy pad contracts and axes/mode/VBlank refresh, CD-ROM real pending/partial-sector checkpoint restore, full-tag toSPR TTE, prior chain/scratch transfers, MFIFO producer wakeup and VIF tag forwarding. The old SQ/LQ micro-test fixture was corrected to enable the lane write mask; a zero mask cannot write the expected lanes.

Modern Wii build: devkitPPC r50/GCC 16.1.0, libogc 3.1.0. `tools/build_r1357.sh` retains EE_BLOCK_FAST, GOURAUD_EARLY_DEGENERATE, ARM_BETA and DSI_FASTMEM build flags. The DOL section bytes match the linked ELF and Wii startup signatures. This is a build/format check, not a physical boot test.

## Wii test

1. Keep R1356 as a control and replace your emulator's boot.dol with this DOL.
2. Use **Cold Boot**, not an older save state. Keep the same optimization settings as the control; the TTE/VU/PAD correctness fixes require no new switch.
3. On the Wii Remote, **MINUS + A sends PS2 START**. A sends CROSS to select Arcade/characters. PLUS pauses the emulator. A Nunchuk is not needed for START.
4. Compare the title black level and whether START reaches the menu, then character selection. Save the new R1357 log if the Wii differs from these native captures.

No BIOS, disc image, SDK or guest checkpoint is included in the public source/build archive.
