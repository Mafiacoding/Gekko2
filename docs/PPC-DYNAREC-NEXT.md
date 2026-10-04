# Own PPC dynarec — next development phase after R1300

Owner direction: pause GX features after this round. Keep the R1297/R1299/R1300 correctness fixes; prioritize stable BIOS/OSDSYS CPU speed before games. No replacement core or full CPU offload is claimed.

## Reference source actually inspected

- Wii64 `r4300/ppc/Recompile.c`: scans branches/delay slots, maps guest PCs to emitted addresses, patches native branch destinations after emission, invalidates guest blocks and performs PPC data/instruction-cache maintenance.
- Wii64 `r4300/ppc/Register-Cache.c`: pairs 32-bit host registers for 64-bit guest values, tracks signed/unsigned reconstruction and dirty state, evicts mappings by recency, and preserves mapping state for conditional fallback paths.
- nullDC4Wii README: describes PPC dispatch, memory fast paths and selective interpreter fallback spills. Its SH4 implementation is only a design reference. These repository claims are not measurements of PCSX2-Wii.

Sources: https://github.com/emukidid/Wii64/blob/master/r4300/ppc/Recompile.c and https://github.com/emukidid/Wii64/blob/master/r4300/ppc/Register-Cache.c ; https://github.com/BenoitAdam/nullDC4Wii . No reference code is copied or integrated here. Preserve licenses if later adopting code.

## Current local limitation and implementation order

Our EE executor caches precise blocks of 2–8 candidate instructions. Generated code calls prepare and retirement helpers around each instruction. The prepare validates live mappings/words; retirement advances timing/interrupts. Most instructions still load/store architectural state rather than keeping a dirty register mapping live across a longer block. Removing these calls without replacing their semantics is unsafe.

1. Measure prepare/retire, dispatch and interpreter fallback separately; use bounded sampling and a diagnostics-off timing comparison.
2. Add a native block execution contract with an event deadline and explicit stop/fault reason. Preserve exact source mappings and self-modifying-code checks. Begin with nonfaulting ALU sequences before memory/control flow.
3. Implement our own liveness/dirty host-register allocator. EE lower 64-bit values use PPC word pairs; preserve upper EE lanes, signed/unsigned results, zero register and HI/LO semantics. Flush precise state before an IRQ, exception or slow helper that observes it.
4. Extend native RAM accesses with guarded address translation and endian-correct loads/stores. Faulting TLB accesses and MMIO use ordered helpers. DMA/CPU code writes invalidate compiled source pages.
5. Link safe block exits and branch-delay-slot paths within the remaining event budget. Never run past EE/IOP interleaving, Count/Compare, timers or DMA deadlines.
6. Apply corresponding R3000A IOP blocks with its separate delay/load/exception semantics; do not transplant R4300/SH4 semantics. VU progress stays independent.

Validation: native architectural oracles, actual generated PPC execution, differential interpreter/JIT state and VRAM, interrupt boundaries, self-modifying code, TLB faults, and then owner Wii logs. Host/PPC instruction counts do not establish Wii FPS. EE/IOP continue to consume CPU cycles even with a complete dynarec.
