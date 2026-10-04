# R1300 — resident GX framebuffer checkpoint

Updated 2026-10-04. Early alpha. R1297 resource-read/sprite fixes and R1299 exact TEV/depth paths are preserved. This round adds a guarded resident framebuffer and reusable decoded source snapshots. After this checkpoint, pause GX feature development and prioritize our own PPC EE/IOP block dynarec, as requested by the owner.

## Implemented

- Compatible opaque PSMCT32/24 flat sprites/triangles and mapped snapshot draws share a full EFB target across guest draws. Exact GS row spans define coverage. No per-draw EFB readback is needed on these resident paths.
- A compact dirty-pixel/alpha plane retains exact PS2 alpha, including CT24 preservation across overlapping CT32 draws. RGB is drawn by GX. CPU metadata still scales with covered pixels; this is not 99% GPU work.
- Compatible scanout backs up raw RGB on GPU, applies the existing 16..240 broadcast clamp before linear filtering, copies the display, and restores the original EFB. It keeps VRAM ownership pending rather than reading/reuploading the framebuffer every presentation.
- CPU reads outside a protected physical framebuffer envelope remain independent. Overlapping reads, writes, raw pointers, target changes and unsupported draw paths resolve the resident surface before access. Failed resolves retain ownership and do not permit stale memory.
- One decoded/GS-shaded texture snapshot is reused when physical source hash, cached CLUT contents, texture state, sampler and mapped crop match. Source hashing uses 32-bit arithmetic on PPC. Alias guards remain. Presentation invalidates this cache because it shares the texture buffer.
- Log lines GX_SURFACE and GX_SOURCE_CACHE distinguish opens, resident draws/presents, final resolves, snapshots and decoded bytes/cache hits. GPU-only snapshots are not CPU VRAM resolves.

Additional buffers: up to 2 MiB resident RGBA8 backup plus about 640 KiB dirty/alpha metadata; existing 2 MiB texture buffer is reused. Allocation failure keeps the bounded fallback. EFB residency requires a validated CT32-compatible layout, width <=640 and the current EFB height. Nonmatching scanout and complex states resolve and use existing paths.

## Explicitly unfinished

True varying Gouraud/textured triangle routing is still software. The mapped triangle API exists but is not hooked to the general GIF triangle rasterizer. Texture functions/filter phases are baked on CPU on cache misses; warm snapshots avoid repeat work. Blended/depth-writing draws retain R1299 snapshot/hybrid handling and may end residency. Alpha metadata, source hashing, row coverage, VRAM imports, GIF and guest CPUs remain CPU work. GX EFB precision still differs from GS. Do not describe this as a complete GPU GS renderer or claim hardware FPS gains.

## Validation

199 native tests pass. Linked Interpreter and JIT ELF checks cover GS memory, deferred ownership/failure retry, old compact-EFB paths, TEV/depth programs and full-VRAM parity. The new residency suite executes the real PPC submission/import code with synthetic libogc/EFB services: 80 resident draws without per-draw wait/copy, one overlapping-read resolve, nonalias reads, CPU-write barriers, CT24 alpha preservation, 11 consecutive source-cache hits and key invalidation, plus GPU scanout clamp/raw restoration while ownership stays resident. See bundled verification results for exit codes.

Synthetic GPU services are not real Wii/Dolphin validation. No new cold boot, Tekken gameplay or FPS result is claimed. The owner should compare SW/GX cold boot and send the new SD log; surface/cache counters expose whether this path is actually used.

## Next CPU work

Build on the existing PPC emitter rather than discarding precise semantics. Start with measured C-call/retirement costs in EE/IOP block execution, register liveness/dirty spills and safe direct RAM loads/stores. Expand blocks and link exits only with precise IRQ/timer budgets, delay slots, faults, TLB and self-modifying-code invalidation preserved. MMIO remains ordered. EE 64-bit/128-bit registers and MMI require our own implementation on 32-bit PPC.

Wii64 is the MIPS-to-PPC reference requested by the owner; nullDC4Wii is another available PPC dynarec reference for dispatch/register/fallback organization. Its SH4 code is not a PS2 core. Lightrec targets PSX and is not a drop-in R5900/VU replacement. Reference repositories must be inspected and licenses retained before any borrowed code. No foreign dynarec core is integrated in R1300. EE/IOP still execute on the Wii CPU; the goal is less interpretation and bookkeeping, not transferring CPUs onto GX.

The next phase is recorded in [PPC-DYNAREC-NEXT.md](PPC-DYNAREC-NEXT.md), including inspected Wii64 files and the current local prepare/retirement bottleneck.
