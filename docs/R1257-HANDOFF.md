# R1257 checkpoint

First BIOS picture: fresh native Interpreter comparison of R1253 and R1256 reports FIRST_RGB at the same 10-million-instruction sampling point, EE=140394434. This confirms delay to Sony startup rather than loss of startup rendering. It does not establish identical Wii/JIT wall-clock timing.

R1257 adds real displayed-RGB detection and boot progress log sd:/pcsx2/R1257-boot.log. The counter remains visible while waiting for the first image. No guest state is forced.

Tekken Tag Tournament SLUS_200.01: fixed MFIFO drain starting on an empty ring. Previously VIF prematurely completed a zero tag and advanced TADR, making the producer believe only one QW was free while waiting for eight. Drain now waits for actual fromSPR producer data, supports ring wrapping and complete inline packet preflight, and resumes after real transfer. This is a subset, not cycle-accurate full MFIFO; MEIS, SPR interleave/destination-chain/TTE remain incomplete.

Fresh Tekken 903973100 EE: REND=760, GIF QW=266475, sprites=98043, HALT=0. Gray rendered display; no Namco logo demonstrated. SPU2DRV generic placeholder replies warrant investigation; do not invent return codes without verifying command contracts.

Fresh diskless R1257 1300177682 EE: HALT=0, GIF QW=7693988, sprites=253147, sampled displayed RGB=0. Genuine OSDSYS Browser/System Configuration still not reached. No JIT performance upgrade claimed.

Validation: 160 regression tests; actual PowerPC ELF checks 53 GS, 19 CLUT, 6 SPR and 9 MFIFO. Both Interpreter/JIT ELF and DOL built with devkitPPC r32. No Dolphin/Wii runtime available.

Checkpoint excludes BIOS, game media and emulator RAM/checkpoint payloads. Patch/diff target original pcsx2-wii-full-debug(1).zip, not an incremental R1256 patch.
