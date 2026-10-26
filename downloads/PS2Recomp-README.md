# PS2Recomp Wii development status

Current source batch: **P19**, `c762b19`.
[Source and P18-to-P19 patch](PS2Recomp-Wii-P19-Source.zip) ·
[Local test status](PS2Recomp-P19-Status.json) ·
[Checksums](PS2Recomp-SHA256SUMS.txt).

P19 is a source update, **not a request for another owner Wii test**. A new
runtime is being held until remaining Wii-target post-loading/rendering behavior is
understood. Physical Wii playability, FPS and a complete game remain unverified.

## New local post-loading evidence

The exact P19 host build passed a 240-second scripted title-to-hub probe:
12 controller packets delivered, required title/hub geometry and terrain
loaded, then 171 nonblack captures. A reviewed capture shows Crash, corridor
and HUD. The runner and game profile reproduce this privately without new
owner Wii log collection. Its gates do not assert image correctness.
The optional rem_pio2f HLE guest stores also pass 645 compiled PPC cases;
that separate endian correction is not claimed as the Crash loading cause.
See `docs/wii/POSTLOAD-VALIDATION.md` in the source package.

## Proven native crash cause

The P17 panic photo exposed modified native instructions. Local reproduction of
the unchanged P17 DOL in Dolphin and native hardware watchpoints identified the
first writer: SDK SRAM DMA. The custom linker placed its buffer at MEM2
`907D66A0`; the linked EXI DMA address mask redirected its 64 bytes into MEM1
native code at `807D66A0`, exactly the reported fault address.

P18 keeps SDK bulk data/BSS in file-backed MEM1 storage and validates SRAM
alignment, code/heap separation and ELF/DOL initialization. It retains cache
fences, exceptions and PPC register save/restore. It also reserves DMA chain
snapshots once, fixes big-endian VU loads/delayed stores and GS HWREG image
bytes, and adds a bounded local Dolphin capture tool. See the source's
`docs/wii/EXI-SRAM-CORRUPTION.md`, `DMA-CHAIN-ALLOCATION.md` and
`VU-MEMORY-ENDIAN.md` for evidence and limits.

A corrected Wii build locally produces a GS lab-scene framebuffer under
Dolphin. That observation does not establish correct rendering of all scenes,
post-loading gameplay, physical Wii behavior or a speed improvement. Test
counts and exact tested revisions are recorded in the status file.

## Build and installation

Use current official devkitPro/devkitPPC and libogc. This batch builds with
**devkitPPC r50 / GCC 16.1.0 / libogc 3.1.0**. The old bundled toolchain archive
is not the tested toolchain. Keep generated owner game code and game assets
private; no disc or BIOS is supplied by the source package.

Keep the working HBC loader `boot.dol`. Future owner runtime updates belong at
`sd:/ps2recomp/crash-runtime.dol`; do not rename the large runtime to the HBC
loader. P16/P17 archives remain for historical comparison; P17 is a superseded
diagnostic and is not a recommended new test.

The separate [guarded native-hybrid compiler draft](https://github.com/Mafiacoding/Gekko2/pull/56)
is still an integration prototype. It is not a complete shared GS/GX runtime or
an automatic ISO-to-playable-DOL converter. Gekko2 main is unchanged by P18.
