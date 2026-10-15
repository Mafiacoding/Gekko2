# R1372 Beta: GX seeded-target ownership

Fixed a reproducible framebuffer ownership defect in R1371. surface_ensure queued the VRAM-to-EFB seed draw without gs_mem_gpu_mark_pending and without invalidating the previous surface snapshot. A later destination-alpha blend rejection could return to software or compact GX with an active, untracked EFB target. CPU VRAM writes then failed to release that target, allowing stale pixels and incorrect target reuse.

R1372 invalidates the snapshot on every new seed, tracks queued texture usage, and publishes pending ownership immediately. A publication failure drains GX and releases the surface. The cold-boot residency fix remains enabled. This is a demonstrated correctness fix, not yet proof of the reported crash's exact cause. No speedup claim.

Validation: 40 native tests passed. Linked PPC regression: an intentionally rejected destination-alpha blend leaves the seed pending, a subsequent software write fences and releases the target, exact software value survives, and a later supported shaded triangle is accepted with correct alpha/depth metadata. The same new test fails against the original R1371 gs_gx.c at the missing pending ownership assertion. Deferred FIFO test checks copy/PixModeSync/sample order and readback ownership. GPU raster output is mocked in these tests. ELF/DOL section identity, bounds and startup signatures checked. Dolphin is not installed here; no Dolphin or physical Wii runtime certification.

Test the ELF in Dolphin with the same BIOS/GX settings as R1371. Start a fresh Cold Boot. Inspect triangle construction, artifacts and the R1372 BUILD line. If Dolphin crashes, save its exception/log details as well as the emulator log. Leave R1370 available as control. ARM worker and EE/IOP/JIT scheduling are unchanged.

Build: DEVKITPRO pointing to devkitPro, bash tools/build_r1372.sh. GCC 16.1.0, libogc 3.1.0, Lightrec pilot, DSI and ARM beta compiled.
