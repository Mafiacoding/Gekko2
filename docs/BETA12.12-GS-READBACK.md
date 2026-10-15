# BETA12.12 – GS Local-to-Host Readback (experimental Wii checkpoint)

- Adds GS VRAM native-PSM 128-bit read FIFO for `TRXDIR=1`, BUSDIR reverse GS bus, writable VIF1_STAT.FDR bit 23 and VIF1 inbound normal DMA.
- Supports PSMCT32/24/16/16S, PSMZ32/24/16/16S, PSMT8/4 and PSMT8H/4HL/4HH, preserving native swizzle and FIFO packing, padding final 16B word only.
- No invented successful readback: if GPU->CPU synchronization fails, no memory is written or completion IRQ raised.
- Tracks DMA MADR/QWC, writes 16B chunks, notifies JIT invalidation and signals DMA completion only once QWC reaches zero.
- Adds `GS_LOCAL_HOST` diagnostic.
- Wii Optimization menu X toggles experimental cross-bit-depth software PSM conversions, saved separately to `sd:/pcsx2/gs-compat.cfg`, applied only on cold boot. Hardware-oriented transfers remain default.
- NOT full IPU MPEG decoder, fully cycle-accurate VU/GIF or full EE/IOP JIT. See STATUS and report; those remain long-term workstreams.
- Native host tests required before use; Wii hardware has not been validated.

## Open hardware coverage (do not mark complete)
- GS->EE reverse transfer: normal VIF1 DMA path only, requires VIF1_STAT.FDR and BUSDIR. Reverse chain-mode DMA and direct GS FIFO CPU accesses are not implemented.
- IPU: MPEG-2 variable-length decoder, macroblock timing, CSC/PACK edge cases and streaming semantics still incomplete.
- VU0/VU1: cycle-accurate Q/P/flags, micro-pipeline and XGKICK arbitration remain open.
- EE/IOP JIT: interpretation fallback remains intentional for unimplemented and precision-sensitive instructions.
- GS aliases: unlike PCSX2, mixed swizzle layouts sharing the same physical eDRAM still need a backing memory/cache-alias reconciliation model.

### Sources examined
https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS/GSState.cpp
https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS.cpp
https://forums.ps2dev.org/viewtopic.php?t=1429
https://github.com/jpd002/Play-

### Test status
- PASS native GS Local-to-Host 32/24/16/4-bit, little-endian packing, padding and bounds.
- PASS native GIF A+D register TRXDIR=1 read FIFO, end-of-transfer reset.
- PASS existing GIF TRXDIR=2 regression.
- PASS BETA12.11 strict and experimental PSM regression.
- PASS Wii cross-compile, no stale BETA12.11 release ID.
- Wii hardware TRXDIR=1 / VIF1-DMA completion, FPS and games: **not yet validated**.
