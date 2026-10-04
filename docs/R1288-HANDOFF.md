# R1288 — GX upload and IOP JIT fallback optimization

Built from the durable, manifest-verified R1287 checkpoint. All prior GS, IOP ELF and DMA memory bounds fixes are retained. The original uploaded archive plus cumulative patch was used to restore the source after the working copy reverted to R1286.

## Changes

GX texture packing processes four adjacent pixels per tile instead of recomputing the tile address for each pixel. Alpha, RGB broadcast clamp, edge padding, capacity checks and live VRAM reads remain identical. GX handles scaling/filtering/output when enabled; this change reduces the CPU work preparing those textures. It does not offload PS2 GS primitive rasterization. GX remains opt-in (Settings RIGHT) and hardware-unverified.

IOP JIT caches unsupported instruction translations by their full 32-bit instruction, independently of PC. A 64-entry direct-mapped cache avoids repeated compilation at new addresses. Exact key checks preserve collision safety and self-modifying guest code. Transient allocation failures remain retryable; reset clears the negative cache. EE/IOP instructions still run on CPU. This is not a full block JIT.

## Verified results

- 185/185 host-native regression tests pass.
- Actual linked PPC ELF: 24 independent GX tiled-byte/capacity/edge/live-VRAM checks per build.
- Exact GX output hashes match R1287. 64x32 packing: 122108 -> 103132 PPC instructions (-15.54%). 640x32: 1207140 -> 1017668 (-15.70%). Both JIT and interpreter builds agree.
- Actual IOP PPC cache test: the same unsupported instruction at 64 PCs takes 64 -> 1 compilation allocations and 12032 -> 3921 PPC instructions. Code replacement and exact-key collisions pass. Allocation/cache services are mocked; counts are not real allocator cost or Wii timings.
- Inherited IOP branch, memory, unaligned access, MMIO and isolation checks pass.
- Actual ELF: 1 valid plus 9 malformed IOP module cases per build, 42 GS bounds cases per build, and 66 full VRAM draw signatures per build unchanged from the verified R1285 baseline.

No new real-Wii FPS, cold-boot, stable OSDSYS or GPU execution result is claimed. GS primitives still render in software. True GX primitive rendering needs coherent VRAM readback and barriers for texture sampling, guest transfers and mixed software/GPU draws before it can safely replace CPU work.

## Install and restore

Use PCSX2-Wii-R1288-Menu-JIT.dol as sd:/apps/pcsx2-wii/boot.dol; ELF is for Dolphin/debugging. Interpreter variants are supplied for comparison. Preserve BIOS/config/discs. Logging uses sd:/pcsx2/R1288-boot.log. No BIOS/game image is included.

Claude.patch and Claude.diff apply to the original pcsx2-wii-full-debug(1).zip source; patch application is byte-verified. The checkpoint includes four binaries, both cumulative patches, tests, measurement JSON, licenses and this handoff. GitHub publication remains pending.
