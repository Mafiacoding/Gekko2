# BETA12.22 – GS/GX formats, XGKICK and IPU

15 October 2026. Branch `codex/gx-xgkick-ipu-recovery`, based on
`c2c5bc471e712e87ca211c532ce8abd445e0240a` (BETA12.21).
Release: `R1389-SPEED-BETA12.22-GS-XGKICK-IPU`.

## Implemented scope

All 13 valid GS texture inputs now decode through the canonical GS sampler
into tiled GX RGBA8: CT32, CT24, CT16, CT16S, T8, T4, T8H, T4HL, T4HH,
Z32, Z24, Z16 and Z16S. Depth textures use Z swizzles and the appropriate
TEXA/AEM expansion. This is conversion into a GX format, not direct use of
GS-swizzled data as native GX texture storage.

GX presentation now handles all four legal color DISPLAY formats:
CT32/24/16/16S. Existing CT32/24 span-read fast paths remain. Indexed and
Z formats are texture inputs, not legal GS FRAME/DISPLAY color formats.

CT16/16S framebuffer targets join eligible compact GX sprite/triangle paths.
GX supplies the source image; late CPU readback applies exact GS packing,
DATE/ATE/AFAIL, FBA, physical-halfword FBMSK, blending and Z updates.
CT16/16S bypass resident CT32 ownership and native EFB blend/depth shortcuts.
Complex, dithered or otherwise unsupported states retain software fallback.
This does not establish universal native GX raster/TEV equivalence.

A differential test exposed a pre-existing software-GS bug: CT16/16S
AFAIL=RGB_ONLY overwrote destination alpha. The software writer now preserves
bit 15, independently checked with both alpha values and FBA values.
CSM2 palette source coordinates no longer erase the separate CSA cache bank;
CT32/16/16S same-format bank retention is checked. Physical cross-format CLUT
storage reinterpretation remains open.

VU1 XGKICK now commits packet length/EOP only when GIF accepts the tag.
Rejected tags remain live if the VU overwrites memory while backpressured;
already accepted tags remain latched. Tests cover the physical ring wrap.

IPU latches decoder DC precision at command start, including later predictor
resets. CTRL writes during decoding cannot alter coefficient scaling.
Soft reset restores default predictors only for latched precision zero.
The typed in-process MPEG state grows by one uint32_t; cross-version raw
state compatibility is not assumed. PCSX2 WriteCLUT and ipuSoftReset were
used as behavioral references, not copied as implementation.

## Failure handling and supplied-log limits

The supplied disc log ends with an explicit EXIT_REQUEST/EXIT_HBC path and
EE/IOP halted=0/0. This supports an application exit-route diagnosis for that
recorded return to HBC, not a proven CPU exception. The B+MINUS+PLUS Remote
shortcut now pauses; explicit HOME+MINUS and menu exit remain available.
CONTROL_ACTION logs include the actual input responsible for explicit exit.

A sticky GX completion failure now pauses emulation and blocks resume/cold
boot in that process, preserving failure evidence and ownership. Shutdown
failure cannot approve a cold boot. Restart from HBC is required. This
contains an observed failure mode; it does not identify the GPU stall root.
Palette, XGKICK and scanout-route diagnostics were extended.

The purple image, unresponsive input, Crash E3 blackscreen and missing BIOS
tiles are NOT confirmed fixed on Wii. The supplied session does not show
IDEC/BDEC/CSC/PACK activity; these IPU corrections are not evidence of the
Crash cause. No BIOS or disc image was available for reproduction. No private
user logs, BIOS, discs or profiles are included in source or deliverables.

## Verification

`python3 tools/test_beta12_22_host.py --output-json <output>` passes all
37 host suites. These include 6,144 depth-texture cases, 832 converted texture
texels, four DISPLAY conversions, eight independent AFAIL alpha checks,
1,024 full-VRAM framebuffer comparisons across four color and four Z formats,
XGKICK live/accepted tags, palette banks and nine IPU reset transitions.
The MPEG coefficient oracle passes 12,201 cases plus invalid-stream checks;
the prior implementation fails six of the newly added precision cases.

Actual linked big-endian PPC execution under Unicorn passes:

- 208 texture texels covering all 13 inputs, four DISPLAY packers and four
  framebuffer imports, including CT16/16S FBA.
- XGKICK live-tag/ring-wrap, independent CSM2 banks and 32 depth texels.
- IPU reset/precision transitions and controller pause/exit mapping.
- GX cold-reset failure injection and token fences, stale callbacks/idle,
  16-bit token wrap and sticky abort failure.
- 36 IDEC cosine-oracle macroblocks, FromIPU CSC DMA/STADR and four REFS selectors.
- 17 VIF DIRECT splits and four software GS framebuffer formats.

Platform allocation, cache services and GPU completion are modeled in these
fixtures. They execute delivered PPC conversion/control code, not a real GX
rasterizer. They are not Dolphin or physical-Wii evidence. The existing
four-combination chain/PSM differential test is also run against this ELF;
all 20 comparisons pass and their final output accompanies the artifacts. No FPS benchmark is claimed.

## Reproducible Wii builds

SDK: devkitPPC r50 / GCC 16.1.0 / libogc 3.1.0, Docker image
`devkitpro/devkitppc@sha256:4c919aa26151dd43d88ca28c922d1fe2409579a8ba60ef56517baf1abdfb1a48`.
Run `bash tools/build_beta12_22_recovery.sh` inside that SDK; use
`CHAIN_RESIDENCY=0` for the compile-time control. Separate build directories
retain the existing SPEED/DSI/AOT/GX/ARM/idle/Lightrec flags, listed in script.
Both ELF-to-DOL checks pass and leave more than the 1 MiB static MEM1 minimum:

| Build | Static MEM1 remaining | DOL SHA256 |
| --- | ---: | --- |
| Chain-capable, runtime factory OFF | 2,703,628 bytes | `41e21307fd1d9bb42cc70d64dedf3784652eab647300001303f95fc415d3ebd9` |
| Compile-time chain excluded | 2,840,364 bytes | `357b242406d241f4c61e8efa8921ef0ced4fa7ea8ef5fc8cfa9b93e71dc10442` |

Static MEM1 headroom is not a runtime heap/stack proof. Both saved experimental
options remain OFF by default and take effect at the next Cold Boot. Ordinary
GS texture decoding is separate from experimental cross-depth local PSM
conversion, which remains opt-in. Previous audited clean 128-bit chains and
PPC save/restore contracts are preserved; general dirty register writeback
and universal native block coverage remain open.

## Hardware validation still required

Install the separate BETA12.22 app directory while retaining the previous DOL.
First test SCPH-50004 BIOS with both experiments OFF, then Crash E3. Preserve
the new session/startup/GX-timeout logs and the exact DOL hash. Test each
experiment separately and together via Cold Boot. A terminal GX failure
requires application restart. Confirm tiles/colors/input and game progress on
Wii before claiming any fix or speed improvement. Full GS feedback/AA1,
physical CLUT aliases, native resident 16-bit TEV paths, VU timing and complete
IPU/MPEG stream coverage remain further work.
