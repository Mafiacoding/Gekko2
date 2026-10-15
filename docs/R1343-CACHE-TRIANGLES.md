# R1343 Beta: distributed EE owner cache and earlier zero-area rejection

R1342 physical Wii session 12 reached FIRST_IMAGE at 486423 ms. It finished with
2,288,740 EE cache collisions, 2,413,128 installed translations and 3,477,056 bytes
in the 6 MiB executable arena. ARM jobs were zero. These counters motivate owner
reuse rather than increasing executable-arena capacity or offloading PPC code.

## Implemented

- Reuse layout: 4096 sets x 4 ways = 16384 owners, versus 1024 x 4 in R1342.
- Fold and multiply the PC before selecting a reuse set, mixing higher address
  bits. Compact/control keep their historical hash and capacities.
- Owner, dispatch, recency and lookup/refusal arrays agree on the new capacity;
  publication/release invalidates the correct hints for both layouts.
- Keep exact virtual PC, source-page epochs, TLB epochs, allocation serials,
  live chain pinning, bounded prefixes and precise retirement contracts.
- Executable arena stays 6 MiB. Static metadata costs 1,536,000 extra bytes.
  Expanding owners can increase cold compilation and executable pressure; a
  physical boot can regress. This is a testable beta, not a promised FPS gain.
- Triangle list/strip/fan assembly classifies zero-area geometry before loading
  and passing colour, UV/STQ, depth and fog arguments. Vertex queue advancement
  and every diagnostic count remain identical. Valid triangles use the existing
  raster/GX path. This avoids useless work; it does not repair VU-generated
  coordinates or fabricate geometry to replace collapsed triangles.

## Actual linked-PPC comparisons

Mocked platform allocation/cache operations; CPU code is from the actual ELF.
Counts are executed PPC instructions, not Wii cycle/time/FPS measurements.

| Replay | R1342 | R1343 |
| --- | ---: | ---: |
| Eight old-hash aliases, 128 eight-instruction grants | 4430603 | 628712 |
| Installed translations in that replay | 128 | 8 |
| Cache collisions in that replay | 124 | 0 |
| 600 vertex kicks / 200 zero-area Gouraud triangles | 104976 | 87760 |

Cache replay retires exactly 1024 instructions. Complete EE state and touched
RAM digest is identical: 0417de86ace6e4581a3612d66b5bcbc4d6d42aad28bd3240bebe1480c5f0ba5e.
All eight source mutations execute their replacement instruction; teardown passes.
Native comparison of 12000 vertex kicks covers independent triangles, strips,
fans, flat/Gouraud and no-draw kicks: complete GIF vertex state, 4 MiB VRAM and all
Gouraud counts match R1342. Signature c7cd3ecae905da4e.

## BIOS source comparison

PCSX2 R5900.cpp executes the real BIOS through its CPU cores. Event deadlines and
EE/IOP synchronisation determine when device work runs. Its EELOAD fast-boot
hooks in x86/ix86-32/iR5900.cpp skip parts of game startup; they do not make each
emulated frame cheaper or replace a correct OSDSYS boot. Instant-DMA BIOS hacks
address particular buffer/timing behaviour, not this EE metadata collision trace.
Reference files are in docs/reference/pcsx2. Public current source checked:
https://github.com/PCSX2/pcsx2/blob/master/pcsx2/R5900.cpp

The previously audited nullDC4Wii block manager retains per-address blocks and
uses a fast address/code mirror. The transferable point is lookup locality and
owner retention, implemented here without importing SH4/PowerVR semantics.
https://github.com/BenoitAdam/nullDC4Wii/blob/26a623e3feca9927d8ffe43706b06d82d672dd64/dc/sh4/rec_v2/blockmanager.cpp

No new BIOS HLE replacement, timing underclock, unchecked VU bypass or guest
frame multiplication is enabled. Such changes would invalidate the comparison.

## Wii test

Keep the R1342 options unchanged, enable Block Cache Reuse, disable EE Compact,
then COLD BOOT / NEW LOG. BUILD must say R1343; EE_CACHE_LAYOUT must report
entries=16384 sets=4096 ways=4. Compare FIRST_IMAGE and the same BIOS stage,
not cumulative EE counts. ARM BIOS RAM jobs may be disabled because session12
completed none. To compare the compact cache separately, enable EE Compact and
cold boot again. A Wii run is still required to establish total performance.
