# GEKKO2 SPEED BETA12.16 — EE exceptions / MMI / GS depth

## Proven scope

Derived from BETA12.15. Five targeted fixes; no claim of a finished EE/IOP emulator or a tested Crash/Tekken/GT3 gameplay boot.

1. **EE exception entry:** EXL=0 saves EPC and BD from the faulting instruction; EXL=1 preserves the original EPC/BD, enters the general exception vector, and terminates an in-flight guest branch. The exception unit is *not* globally precise for every opcode or CPU stage.
2. **128-bit PPC dynarec instructions:** native lane-level code for PCGTW, PMAXW, PCGTH, PMAXH and PCGTB (signed comparisons and maxima). This is instruction coverage, **not a complete 128-bit register allocator**; full liveness, helper contracts and residency remain open.
3. **JIT event successor:** a learned link is reused only if the serial/function/PC, mapping and RAM-page generations, eligibility, block budget and event entry all remain valid. If stale, it now performs the normal variant lookup rather than silently refusing to chain. A complete whole-system proof of invalidation across DMA/MMU/self-modification is not claimed.
4. **GS Z-depth width:** Z16/Z16S and Z24 fragment depths are saturated to their legal range before compares and writes in CPU raster and relevant GX writeback paths. GS raw VRAM transfers are untouched.
5. **Existing scheduler/GX cache optimizations retained:** the prior EE/IOP event scheduler and GX residency/cache paths are unmodified in this release; no new timing guarantee or complete alpha/blend/framebuffer feature is claimed.

## Host tests and Wii build

- `tests/test_beta12_16_codegen.c`: **500,015 checks, 0 failures**, validating MMI decode/codegen and lane compare/max reference. These are host-side checks, not execution of PPC machine code on a real Wii.
- `tests/test_ee_exceptions.c`: **0 failures**, adds nested EPC/BD and branch cancellation checks.
- `tests/test_gx_resident_depth_r1330l.c`: **173 checks, 0 failures**, including Z16, Z16S and Z24 clamping.
- `tools/test_beta12_15_hotpaths.sh`: three retained regression groups **PASS** (IOP tick cadence, JIT cache eviction, GS source digest coherency).
- `tools/build_beta12_16.sh`: Wii cross-build with gcc 16.1.0, libogc 3.1.0 and inherited SPEED/DSI/GX options; validated DOL and matching ELF with `tools/verify_dol_r1352.py`.
- `tools/verify_wii_memory.py`: **2,857,004 static MEM1 bytes remain**, minimum configured requirement 1 MiB.
- Compiled DOL has the unique checkpoint `R1389-SPEED-BETA12.16-EE-MMI-GS-Z` and contains no older BETA12.15 version strings.

## Unresolved engineering areas

- Accurate EE exceptions for all AddressError/Overflow/Coprocessor/Interrupt cases, alignment, nested fault semantics across all possible execution modes and JIT helpers.
- Broad 128-bit liveness/register-allocation and native encoding coverage beyond the five implemented MMI opcodes.
- Complete native-edge invalidation proof under TLB/ASID/DMA changes and self-modifying code; finer IOP/EE event deadlines and software/hardware timing.
- Full GX depth/stencil/alpha blending, texture alias, local-to-host reverse-chain-DMA, feedback loops and partial framebuffer resolves.
- IPU MPEG-2 and VU1/XGKICK cycle accuracy, required for broader game compatibility.
- **Real hardware test is pending**. No gameplay, FPS or boot-time improvement is claimed for this version. The diagnostic build uses inherited `-O1` compiler settings; do not compare directly against an optimized BETA12.7 `-O3` FPS baseline.

## Build/use

With a compatible devkitPPC/libogc environment, run `tools/build_beta12_16.sh`. Place `apps/gekko2/boot.dol` from the SD package at `SD:/apps/gekko2/boot.dol`. Do not bundle Sony BIOS or PS2 game images with releases.
