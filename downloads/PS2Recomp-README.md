# PS2Recomp Wii experimental owner build

Current batch: **P20 GS accuracy**, source `5154c62`.
[Protected owner update](PS2Recomp-Crash-Wii-P20-GS-Update-PASSWORD.zip) ·
[Source and P19-to-P20 patch](PS2Recomp-Wii-P20-Source.zip) ·
[Test report](PS2Recomp-P20-Status.json) · [Checksums](PS2Recomp-SHA256SUMS.txt).

**This is an experimental Crash E3 owner candidate, not a finished alpha or a
claim that all graphics problems are fixed.** No disc or BIOS is supplied.
The protected archive uses the existing privately shared owner password.

## Installation

Replace only **`sd:/ps2recomp/crash-runtime.dol`** from the inner ZIP. Keep the
working HBC loader **`boot.dol`**, the owner game assets and a backup of the old
runtime. Do not rename the large runtime to the HBC loader. Old P16/P17 packages
remain historical comparisons; P17 is a superseded diagnostic.

## Implemented GS corrections

- COLCLAMP signed blend wrapping/saturation and 16-bit DTHE/DIMX dithering.
- SCANMSK even/odd color and depth suppression.
- Sprite fractional UV, flat second-vertex Q, reversed texture axes, and negative
  nearest texture coordinate rounding.
- Fractional triangle XYOFFSET and exact single-owner shared triangle edges.

The P18 correction for SDK SRAM DMA overwriting native MEM1 code is retained,
as are bounded DMA snapshot allocation and earlier VU/HWREG endian fixes.
P20 changes the PS2Recomp software GS path, not Gekko2's separate GX renderer.

## Evidence and limits

All **80 GS / 42 VU** test groups pass. The old renderer passes its original
74 groups and fails all six new groups. Compiled PPC750 tests check **320
triangle tilings / 20,480 pixel ownership cases**. The 240-second host scene
probe reaches hub terrain/geometry and subsequent GS frames; a reviewed capture
shows Crash, corridor and HUD. A separate scalar SIMD/ASan run reports no error.

The exact P20 Wii build produces a dark lab-scene framebuffer in local Dolphin;
the sampled code region stays unchanged. That narrower observation does not
prove post-loading gameplay on the Wii-target binary. ELF/DOL, MEM1, loader and
SDK DMA address checks pass. Physical Wii behavior and FPS remain unverified.

GS raster sampling/attribute DDA precision, mipmap/LOD, AA1 and complete visual
correctness across games remain incomplete or unverified. Audio/MPEG and other
device features also remain open. See the source's `docs/wii/GS-PIXEL-ACCURACY.md`
and detailed status file. No claim of full Wii playability or speed gain.

## Build

Use current official **devkitPro/devkitPPC and libogc**. This batch is tested with
**devkitPPC r50 / GCC 16.1.0 / libogc 3.1.0**, not the old bundled toolchain.
Keep generated guest functions, owner assets and raw game captures private.
Preserve upstream license notices when sharing the source.

The separate [guarded native-hybrid compiler draft](https://github.com/Mafiacoding/Gekko2/pull/56)
remains an integration prototype, not an automatic ISO-to-playable-DOL converter.
Gekko2 main is unchanged by this batch.
