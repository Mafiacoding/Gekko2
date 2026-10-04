# Gekko2 roadmap — R1302

The [current status](../STATUS.md) separates implementation, native tests and real-Wii observations.

## 1. Stable BIOS on Wii

Confirm repeated cold boots, first-image timing and OSDSYS Browser/System Configuration navigation. Compare paired JIT/interpreter builds with identical BIOS and settings. Keep the single experimental GX option and legacy SD paths.

## 2. Custom PPC dynarec — current priority

- Profile current EE/IOP/VU and GS costs on Wii before selecting hot paths.
- Extend register residency and build a liveness/dirty-register allocator.
- Add guarded memory/control blocks with exact exception/delay-slot behavior.
- Add corresponding IOP blocks without changing device interleave or interrupts.
- Add safe block links and code/TLB invalidation; never free executing code.
- Improve remaining VU pipeline/flag/Q/P/EFU cases beyond guarded straight-line blocks.

Wii64 and nullDC4Wii are references only. No foreign core or Lightrec integration is claimed. EE/IOP execution remains CPU work; GX cannot run arbitrary MIPS instructions or replace guest I/O scheduling.

## 3. Graphics after CPU work

GX feature development is paused at R1300. Preserve resident framebuffer/VRAM ownership, readback barriers, destination alpha, texture aliasing and software fallbacks. Later work includes varying Gouraud/textured triangles, broader reusable textures and framebuffer formats, and measured GPU batching.

## 4. Compatibility and alpha

Revisit post-Namco Tekken and GT3 blockers with reproducible fresh boots after BIOS stabilization. Publish paired builds with source, checks and clear limitations. No release date, supported-game list or hardware FPS promise yet.
