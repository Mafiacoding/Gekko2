# Gekko2 SPEED BETA12.15 – EE JIT, IOP Scheduler, GX texture-cache policy

Based on R1389-SPEED-BETA12.14-BATCH1-5; the five prior batches, the
verified INTC/VBlank polling acceleration, GS Local-to-Local and
Local-to-Host transfers, and alias handling remain in place.

## New changes

1. **EE precise-block cache lookup**: one consistent cache-reuse/compact
   option snapshot drives hash stride, set selection and way count. This
   removes repeated option/stride queries in a particularly frequent JIT
   dispatcher path, while retaining the previous PC hash, generations,
   rejection, owner eviction and reuse. Old single-argument helper preserved
   for the existing collision/eviction test fixtures.
2. **IOP scheduler**: one-tick grants now directly call the existing native
   one-tick owner. Its AOT → JIT → scalar fallback and before-tick callback
   order are identical; larger grants still use deadline-safe batching.
3. **GX decoded texture reuse**: GX_InvalidateTexAll is required after the
   CPU flushes newly packed GX texture bytes, but not on a cache hit that
   has left the same GX texture buffer unchanged. The pending-upload bit
   survives rejected draws, so the first real consumer still flushes and
   invalidates. New log `GX_TEXCACHE_INVALIDATE_SKIPS` counts avoided
   invalidation calls.

## Verified

- Real devkitPPC 16.1.0 / libogc 3.1.0 cross-build, with original
  SPEED / DSI / GX flags; all objects built cleanly in build-beta12-15.
- DOL–ELF byte-level verification passes; static MEM1 remaining 2,858,028
  bytes; new DOL includes correct BETA12.15 runtime version identifier.
- `tools/test_beta12_15_hotpaths.sh`: IOP budget/callback/idle/halt,
  EE code-cache eviction and serial generations, and GS hash/cache
  invalidations across PSMs / GPU-failure paths pass. The CHD stub in
  this host-only suite is **not** used in the Wii binary or CHD testing.

## Limits / known open work

- No physical Wii test or measured FPS improvement for BETA12.15 yet.
- The existing `-O1` cross-build is a diagnostic comparison point and
  cannot be compared directly to historical `-O3` runs.
- Dynarec remains incomplete for several instructions and precise
  exception/VU timing cases. IOP instruction scheduling is not cycle-exact;
  direct block links still require generation barriers and invalidation.
- GX does not yet reproduce all GS blending/depth/alias behaviors.
- Crash E3 in-game remains unconfirmed; Tekken and GT3 bottlenecks need
  post-change Wii logs. No fake VBlank or synthetic GS frames added.

## Research sources

Dolphin's JIT source and performance reports discuss avoiding repeated block
lookup and invalidating generated code on writes; this change is limited to
removing duplicate lookup setup, not removing Gekko2's safety checks.
Dolphin JIT: https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/Core/PowerPC/Jit64/Jit.cpp
Dolphin lookup profile: https://dolphin-emu.org/blog/2023/11/25/dolphin-progress-report-august-september-and-october-2023/

