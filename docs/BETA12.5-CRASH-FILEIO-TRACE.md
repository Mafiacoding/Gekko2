# BETA12.5 — Crash E3 FILEIO transport investigation

Date: 2026-10-14. Not pushed to GitHub main.

## Verified from owner's full BETA12.4 Wii log

- The one-shot diagnostic fired once, at log line 24,908:
  `CRASH_E3_NUERROR pc=00103d74 sp=01ffd860 caller=0012fa18 provider=30313132 heading="NuError - nufont.c(117) : " detail="unable to create font fonts\ariel32"`
- `30313132` is little-endian ASCII for **2110**, the old Sony FILEIO protocol returned by the selected IOP module.
- The demo intentionally loops at `0x00103D70/74` after reporting failure; GS `PMODE=0`, `FIRST_IMAGE=0`, `GX ready=0` are effects, not proof of a renderer defect.
- `ISO_CACHE requests=1993 hits=1845 fills=148 physical_bytes=5524544 failures=0`. This validates low-level sector reads only, not file OPEN/READ semantics.
- The entire log lasted ~73.4 min, but the error occurred at around EE 950 million instructions, ~18 min.

## Diagnostic implementation

This build does not bypass `NuError` or fabricate any font asset. It exposes a read-only
`ee_core_fileio_trace()` snapshot outside the serialized guest EE state and records:

- `FILEIO_TRACE` at changed FILEIO-RPC activity: `version`, `versions`, `opens`, `ok`,
  `fail`, `reads`, `bytes`, `closes`, `font_opens`, `font_ok`, `font_fail`,
  `font_reads`, `font_bytes`, the last path and last reply/read sizes.
- `CRASH_E3_FILEIO` once at the already verified NuError stop, alongside the existing
  `CRASH_E3_NUERROR`. Filename copies sanitize control characters; the emulator
  does not alter any guest register/memory or the original RPC replies.

**Interpretation:** if `font_opens=0`, look for an alternate service ID or path; if
`font_fail>0`, investigate path/ISO mapping and old OPEN request layout; if OPEN
succeeds but `font_bytes=0` or short, inspect READ payload structure and delivery;
if both FNT/BMP reads succeed, investigate font parsing/texture allocation.

## Verification

- C translation units: native syntax check passed.
- Wii cross-build: passed with provided devkitPPC 16.1.0/libogc 3.1.0.
- DOL/ELF structural and MEM1 checks: passed, 2,776,844 bytes free.
- Toolchain note: the initial -O3 build exceeded the per-command limit while
  compiling vendored GNU Lightning; on the successful continuation the remaining
  Lightning compilation used -O1. Core objects had already compiled using -O3.
  Thus this temporary binary is intended for functionality tracing, **not FPS benchmarking**.
- No real Wii gameplay success claimed; BETA12.5 has not yet been run on hardware.

Keep the private demo BIN, BIOS, and session logs outside this source archive and git.
