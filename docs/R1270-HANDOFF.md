# PCSX2-Wii R1270

R1270 extends the verified R1269 baseline. This is a working checkpoint, not a completed full EE/IOP/VU JIT port. No physical Wii/Dolphin FPS measurement is available.

## Changes

- Nine EE native encodings: MFSA, MTSA, MTSAB, MTSAH, QFSRV, MADD, MADDU, MADD1, MADDU1. Full register aliasing, SA byte shifts, selected accumulator pipeline and preserved register halves tested.
- Twenty VU upper encodings: twelve MIN/MAX forms and eight ITOF/FTOI forms. Native VU upper coverage is now 95 encodings; lower coverage remains 33.
- MIN/MAX uses raw signed bit ordering reversed for two negative operands, following bundled primary PCSX2 VUops.cpp fp_min/fp_max. The macro and micro interpreters match, including signed zero and NaN bit patterns.
- ITOF uses native PPC double-bias conversion and float32 rounding, removing the C helper call. FTOI preserves exponent-based saturation and canonical VF0 input. Micro FTOI no longer performs undefined out-of-range C casts.
- Unsupported EE cache fixture now uses PMADDW because QFSRV is supported.

## Validation

- 180/180 native regressions.
- 14,400 generated PPC cases for nine new EE encodings, independent integer oracle and full context/ABI checks.
- 6,080 upper plus 5,280 lower VU PPC cases against the micro interpreter: masks, aliases, integer edges, random raw MIN/MAX/conversion inputs, infinities, NaNs, signed zero, local memory, branches, ABI.
- Both actual Wii ELF builds pass inherited EE/IOP/VU/cache/IRQ/MMIO tests and new frontend acceptance for nine EE and twenty VU encodings. Raw emitter semantics are tested separately; acceptance tests do not claim a full BIOS run on PPC.
- Authentic SCPH-50004 native interpreter continuation: Cross opens Browser, release, Circle returns, release. EE 1,437,550,160 to 1,558,401,798 (120,851,638 additional instructions), halt=0. Browser and menu screenshots inspected. No guest PC/RAM/menu flags forced. This resumes an existing BIOS checkpoint; it is not a new cold boot.

## Build and reproduce

Use tools/build_r1270.sh with the supplied devkitPPC/libogc, DEVKITPRO and DEVKITPPC set. Build is serialized (-j1). verify_regressions.py runs the native suite. verify_jit_ee_sa_acc_r1270.py and verify_jit_vu_micro_r1270.py require Unicorn Python. verify_ppc_extensions_r1270.py takes an ELF and --nm path and tests the real Wii binary.

## Remaining work

True EE/IOP/VU block execution and linking, register allocation and event-boundary correctness remain open. EE packed multiply and other declined paths, IOP HLE-sensitive calls/exceptions/load-delay accuracy, VU MAC/status flags, Q/P readiness, EFU/random paths and parallel pipeline hazards are not complete. Existing micro arithmetic is the simplified model; these results do not establish full PS2 floating-point/timing accuracy. No new Tekken image or FPS result is claimed. R1269 remains a safe fallback. BIOS, games and private RAM checkpoints are excluded from the public package.

The Claude patch and diff are identical cumulative unified patches against the original pcsx2-wii-full-debug(1).zip tree. Exact dry-run/apply and byte comparison are performed when packaging.
