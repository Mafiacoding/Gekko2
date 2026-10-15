# BETA12.6 — Crash E3 FILEIO Open packet offset fix

The real Wii BETA12.5 log (40,346 lines, 28m50s) shows Sony FILEIO `2110` being recognized, followed by **six failed OPENs**, including **three failed `ARIEL32` font opens**, with zero READs and `-ENOENT (-2)` returned. The observed corrupt paths include `cdrocdrom0:\NU2.ERR;1`, `h???cdrom0:\GFX\PARTICLE.RAW;1`, and `????cdrom0:\FONTS\ARIEL32.BMP;1`.

The previous parser guessed only a pathname at byte offset 0 or 4. In the observed old game request, the device string begins at **offset 8** of the SIF FILEIO open payload. This release recognizes exact device-prefix matches at word-aligned offsets **0, 4, 8, or 12**; unrecognized packets retain the existing +4 fallback and normal error handling. A successful OPEN is still contingent on the mounted ISO containing the file; the fix does not invent data or bypass `NuError`.

`FILEIO_TRACE` and `CRASH_E3_FILEIO` now include `open_offset` for future Wii validation. The new `test_fileio_open_packet_beta12_6` tests synthetic 0/4/8/12 request layouts, and optionally tests the real owner's mounted demo ISO for both Arial font entries (the private disc must never be shipped).

**Status**: Host ABI and disc lookup are independently tested; Wii runtime gameplay / actual successful font OPEN or READ remains to be tested. GitHub main unchanged.

## Verification results (this session)

- BETA12.5 real Wii log: `FILEIO_TRACE` at line 25,028 reported `opens=6 ok=0 fail=6 reads=0`, three font opens failed with -2; `CRASH_E3_NUERROR` at line 25,026 repeated `nufont.c(117)`.
- New synthetic OPEN packet offset test: **PASS** for offsets 0/4/8/12; unknown prefix falls back without fabricating a match.
- Real owner Crash E3 raw Mode2 CD image mounted in the host test: **PASS** for `FONTS\ARIEL32.FNT;1` at LBA 763, 2,208 bytes, and `FONTS\ARIEL32.BMP;1` at LBA 746, 32,886 bytes. Game disc not included in downloads.
- Existing host regressions: fileio protocol 2110/2050, ISO nested path, EE boot RPC error handling, NuError probe, synthetic ELF guard: **PASS**.
- Wii devkitPPC cross build: **PASS**; `verify_wii_memory` PASS (2,735,340 bytes statically free); `verify_dol_r1352` PASS; SHA-256 `a43c415756802696c711790031ae604c97d02ba35666ea53e9f1205721da8e38`.
- Build caveat: first fresh -O3 build was interrupted by tool time limit in GNU Lightning compilation; completing missing objects at -O1 and linking the already built core worked. This experimental DOL is not a speed benchmark. No Wii runtime post-fix test has been conducted.
