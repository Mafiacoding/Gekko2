# R1297 — Tekken resource reads, sprite coverage and COP1 blocks

2026-10-04. Early alpha; stabilize BIOS/OSDSYS and measure hardware before broader game claims.

## Reproduction and root cause

The actual supplied CD BIN identifies itself as NTSC-U retail SLUS-20001. Freshly rebuilt R1296 native sources reproduce the EE TLBL around 694 million instructions at PC 00345534, BadVA 14791102. The game dereferences a resource pointer generated from empty buffers; no TLB exception is suppressed by this fix.

Its 66,077-byte RSPU2DRV.IRX has FNV-1a32 e16bba10. Dispatch +0608/+0640 invokes +2DFC with mode 0/1. Decode relative sector by subtracting 4820h and byte count by subtracting 350Ah; mode 1 rounds bytes to 64. TEKKEN.BIN supplies the actual bytes from the mounted disc.

204E mode 0 performs an EE bus-RAM transfer (uncached bus aliases do not invoke guest load translation). 2045 mode 1 performs a SPU2 sound-RAM transfer. Worker +224C sets the transfer start address and invokes SpuWrite. An intermediate development implementation wrongly wrote IOP RAM; it was rejected by longer testing and corrected before this release. The synthetic regression explicitly checks IOP memory preservation. Do not route sound banks into IOP executable RAM.

Only the exact verified module enables this custom synchronous HLE protocol. Read/buffer/file failures report failure instead of success; no game bytes, BIOS, IRX, RAM dumps or private runtime checkpoints are included. Real worker execution, sound playback and DMA timing are not claimed.

## Sprite coverage and GX

Namco draws strips ending at half-pixel coordinates (63.5, 127.5, etc.). Flooring both endpoints omitted each last column. Coverage now uses [ceil(min), ceil(max)) after XYOFFSET, retains the original 12.4 positions for texture gradients/prestep, and handles reversed corners. Flat GX submission receives the same corrected bounds. Textured Namco sprites still use software rasterization and GX presentation when enabled; this is not a textured-GX port.

GIF checkpoints append one raw sprite-position field. Loading the previous shorter GIF chunk reconstructs integer positions without shifting old fields. Native tests cover 128 fractional flat/UV/STQ cases and new/legacy split-sprite checkpoint continuation.

## JIT extension

Shared block policy now allows existing native COP1 MFC1/CFC1/MTC1/CTC1 and ABS/MOV/NEG operations. No trapping arithmetic, memory or branch instructions were newly admitted. Linked PPC tests preserve full bit patterns including NaNs, signs and zeros and stop at the precise Count/Compare interrupt boundary.

Eight retired mixed COP1/ALU instructions take 4,659 interpreter versus 3,226 warm block PPC instructions in the isolated mocked-platform test (30.8% fewer). These counts are not Wii FPS measurements. Complete memory/control blocks, linking/register allocation and full EE/IOP/VU architectural coverage remain open.

## Validation and limits

196/196 native tests pass. Both DOL/ELF engine pairs build with devkitPPC/libogc. Linked PPC jobs cover COP1 blocks, integer retirement/fallback, packed context and GX gates/readback with mocked GPU/cache services; sixteen fractional flat-sprite phases check actual GIF-to-GX bounds. Physical GX rasterization is not simulated.

Fresh native Tekken boot reaches 1.2 billion EE instructions, with EE/IOP running and the previous BadVA absent. The six decoded resource tables contain valid headers and pointers; subsequent disc reads occur. The captured Namco image is strip-free. No 3D gameplay is demonstrated, and post-logo progress needs further investigation: the observed PC 00400660–00400678 polls INTC_STAT bit 2 (VBlank-start); distinguish flag acknowledgement/interrupt delivery from renderer progress before changing timing. Native boot speed is not Wii speed. A new Wii log is pending.

Use tools/build_r1297.sh with DEVKITPRO/DEVKITPPC configured. Run tools/verify_tekken_namco_r1297.c against privately supplied BIOS/disc for the diagnostic cold boot. It sends normal START during 400–450 million instructions and never forces guest PC, registers or GS output. It saves a logo image at the first sampling boundary after 695 million instructions and checks both cores for halt.

## Delivery and next work

Copy PCSX2-Wii-R1297-Menu-JIT.dol to sd:/apps/pcsx2-wii/boot.dol. Interpreter binaries are included for comparison. SD logs use R1297-software.log / R1297-gx-render.log. Preserve the single experimental GX option and first-image/state gates.

Cumulative Claude patch/diff apply to the original pcsx2-wii-full-debug archive. The checkpoint includes source, binaries, public verification and screenshots. It is closed and CRC/hash checked before publication; the restored prior checkpoint had a truncated ZIP directory, so do not reuse that damaged package as the baseline.

Next: inspect the new Wii log for boot timing and GS_ROUTE/GS_DETAIL; isolate the post-Namco guest wait; extend precise memory/control JIT and textured GX only with exception, texture/alpha and VRAM-alias parity tests. Do not advertise a complete port or FPS target.
