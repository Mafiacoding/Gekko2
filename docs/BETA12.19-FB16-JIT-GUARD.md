# Gekko2 SPEED BETA12.19 — CT16/CT16S framebuffer and EE JIT guard

## Basis / goal
Based on BETA12.18; preserve existing VBlank fast path, GS alias/readback, SPEED/DSI flags. Fix identified raster correctness issues without fabricating image frames. The privately supplied `crash.bin` and PS2 BIOS are **not** distributed.

## Implemented

1. **GS software framebuffer writes**: PSMCT16 and PSMCT16S now use their native 16-bit swizzled memory views, not the 32-bit framebuffer view. RGB is packed to 5:5:5, output alpha goes into bit 15, and FBA sets bit 15. `FRAME.FBMSK` selects the physical low or high halfword using the actual swizzled pixel address. DATE retains per-pixel alpha rejection. Destination RGB and destination alpha for blending are read from native 16-bit content; CT24's blending destination alpha is fixed at 0x80. PSMCT32/24 continue through the pre-existing branch. Alpha-test `RGB_ONLY` preservation remains restricted to 32-bit output; CT16 correctly behaves as framebuffer-only.
2. **GX import Z write consistency**: Z16, Z16S and Z24 writes are saturated to the same range already used for depth testing. The scalar raster path also clamps its final Z write, so direct slow-fallback calls cannot disagree with the GX path on overflow.
3. **GX framebuffer helper**: `gs_gx_framebuffer_store` now supports 16-bit packed alpha correction and masked merge for the appropriate already-selected 16-bit mask. No blanket enabling of the native GX path for unsupported DATE/FBMSK combinations.
4. **EE JIT dispatcher fault fences**: outer cached-successor dispatch, event-successor dispatch, initial compiled execution and chain entry decline immediately when `exc_raised_this_step` or `mem_tlb_miss` is set. This prevents a second speculative native entry during an outstanding exception, complementing BETA12.18's per-instruction preparation guard.
5. Unique BETA12.19 binary/log identification (no BETA12.18 string in resulting DOL).

## Verification

- `test_beta12_19_framebuffer16.c`: 16 checks, zero failures (CT16/16S physical addressing, FBA, physical-halfword FBMSK, DATE, Z16 saturation, GX helper).
- `test_gx_pipeline_r1298.c`: 324 end-to-end synthetic host cases, zero failures.
- BETA12.17 32 depth checks and 7 framebuffer alpha/mask checks: passed.
- `test_beta12_19_dispatch_guard.py`: 4 source-order checks passed; this is **not** a substitute for Wii/native PPC exception simulation.
- Wii devkitPPC 16.1.0 + libogc 3.1.0 cross-build with original SPEED/DSI flags: passed.
- ELF vs DOL sections: passed. Static MEM1 remainder: 2,853,260 bytes.
- **Not yet run on physical Wii.** No confirmed Crash E3 gameplay, no measured new FPS, no guaranteed fix for its CT32 frontbuffer blackout.

## Still open

- The Crash E3 32-bit selected frontbuffer becoming black; early title requests include a missing `TITLE.LGT` on that demo CD image. This is not proven to be the blocker.
- PPC host-register pinning across fully independent JIT callable blocks (requires a shared ABI, dirty-register flush on helpers, host calls, exceptions, DMA invalidation and code-generation changes). Current residency remains within compiled blocks and canonical guest state at boundaries.
- Remaining GX DATE/alpha-test, blend and framebuffer alias cases, VU1/GIF, CDVD and PS2 timing. Native GPU shortcuts remain guarded by software fallback.

## Hardware logging

Expected SESSION checkpoint: `R1389-SPEED-BETA12.19-FB16-JIT-GUARD`.
Test with private legal BIOS/media; compare the same game, settings, and time interval against BETA12.18. Inspect `GS_SCANOUT_FORMAT`, `GS_BUFFER_COMPARE`, `GX_PIPELINE`, and EE exception state. For Crash E3, missing `TITLE.LGT` remains a separate guest-resource observation.
