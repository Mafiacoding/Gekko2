# Gekko2 R1378B: GS correctness + CHD beta

## Graphics
Retains R1376B sparse 4x4 seeding, bounded readback and resident completion fences, plus R1377 shared-edge coverage correction and GS_SOURCE_RGB.

Implements TEST_1/TEST_2 DATE/DATM, context switching, CT32 alpha bit31 and CT16/CT16S bit15 rejection. Rejection suppresses color and Z writes. DATE with CT24 rejects the draw, consistent with PCSX2's documented PS2 test. DATE draws use the coherent CPU path; native GX admission and opaque CPU fast paths cannot bypass this test. GS_DATE logs active bits, tested and rejected pixels. This log shows whether BIOS actually uses DATE; we have not established that DATE caused the previously missing cubes.

Nearest texture sampling now selects floor(u), floor(v), then applies configured clamp/wrap. It no longer rounds up fractional texels or replaces negative coordinates with zero. GX sprite map preparation uses the same floor convention. Avoids an unrelated CT32 VRAM read before indexed texture/CLUT sampling, which could induce unnecessary coherence work.

Reference consulted, no PCSX2 implementation copied:
https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS/Renderers/HW/GSRendererHW.cpp (CT24 DATE hardware test)
https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS/Renderers/SW/GSDrawScanline.cpp (16.16 texture coordinate arithmetic shift before clamp/repeat)

The PCSX2 reference image is a visual target. No identical BIOS geometry, darkness, GPU pixel result or FPS claim is made. Need R1378 Wii log plus a same-phase picture. No arbitrary gamma/brightness boost or skipped geometry commands.

## CHD
Integrated libchdr v0.3.0 and bundled miniz, LZMA, zstd and dr_flac sources/licenses. Read-only hunk cache, at most 1 MiB user decode buffer per image (plus libchdr metadata/map/codec allocations). No whole-game RAM unpacking or SD extraction.

Case-insensitive .chd selector, magic detection, ISO9660 file lookup and live CDVD sector reads. Supports 2048-byte-unit DVD/raw CHDs and single data-track 2448-byte-unit CD CHDs with zero stored pregap/postgap. First-track ISO PVD validates payload offsets 0/16/24. Multiple tracks/audio/pregaps, parent/differential CHDs and other layouts are rejected instead of fabricating a sector map. Typical CHDv5 tested; not every historical CHD variant.

Wii newlib off_t is 32-bit: compressed physical files >=2 GiB remain unsupported. Added checks in libchdr so physical seek offsets are rejected instead of truncated, and negative/failed file-size queries fail opening. Decompressed logical offsets use uint64_t. Large logical disc size is distinct from compressed physical size. This is not a split-file reader.

CHD_READER log counts requests, decoded hunks, hunk reuse and decoder failures. Decoding adds CPU cost; storage reduction is content/codec-dependent, not a promised speedup.

## Validation
46 native regression programs pass. New DATE tests cover DATM pass/fail, rejected color/Z preservation, CT24 rejection, CT16/16S alpha-bit rejection, both contexts and opaque admission. Nearest tests cover fractional selection, negative REPEAT, prepared sprite indices. 19,584 shaded CPU/GX-source parity pixels pass.

Synthetic non-game disc fixtures created with chdman 0.251: none/zlib/lzma DVD-unit and CDZL single-track CD CHDs. All 128 sectors read in random and repeated order match ISO exactly; directory/file lookup, live CDVD mount/read/unmount, LBA boundary and truncated-file rejection pass. Final libchdr error-check change retested with these fixtures. Synthetic inputs included for reproducibility; no BIOS/game supplied.

Linked PPC tests with mocked GX pass sparse seed preservation, partial readback, exact alpha/depth metadata and resident fences. DOL/ELF bytes and MEM1 layout validate. Physical Wii GPU pixels, real-game CHD playback, speed and long-run stability remain unverified here.

## Wii test
Save current log. Replace boot.dol with R1378B and cold boot, using the same graphics settings. Test BIOS visuals first, then select a supported .chd through the game browser. Save logs with GS_DATE, GS_SOURCE_RGB, CHD_READER and GX_SPARSE_SEED. Use fresh boot rather than resuming an old raw-memory checkpoint.
