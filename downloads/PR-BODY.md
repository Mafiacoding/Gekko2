Tekken's texture triangles previously fell back when combining 128×1024 textures, alpha tests and interpolated Z writes. BETA12.25 adds an experimental GX RGB path for these states, preserving exact CPU GS fragment permissions and canonical alpha/Z shadows. It also admits Gouraud DECAL, supports PABE for the admitted alpha-blend equation, and caches decoded sources with VRAM/palette/state invalidation.

The build flag `GEKKO2_GX_EXTENDED_TRIANGLES` and separate control target allow a reproducible comparison. Existing SPEED/DSI/AOT flags, full-size presentation, fault handling and standard-OFF Chain/PSM settings remain. Unsupported fog, mipmapping, region clamps, complex blends, masks, feedback and other GS states keep software/hybrid fallbacks.

Validation: 52 host suites; 81,920 new direct software-GS permission comparisons and 24,576 permission/bounds combinations; linked PPC checks for fragmented alpha coverage, variable Z, DATE, ZB_ONLY, PABE, source reuse and 128×1024 upload admission. Existing linked PPC shading, formats, fence and cold-boot checks also pass. Both devkitPPC DOL/ELF and static MEM1 checks pass. GPU calls in PPC tests are mocked; no real-Wii RGB/FPS, Tekken gameplay or question-mark tile correction is claimed.

See `docs/BETA12.25-GX-TRIANGLES.md` for coverage, remaining gaps and test commands. No private assets or user logs included.
