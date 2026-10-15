# R1344 Beta — reclaim EE owners before publication; defer impossible retries

## Physical evidence motivating this fix

The R1343 BIOS session13 reached FIRST_IMAGE at 637131 ms, versus R1342 session12
486423 ms (31.0% longer). At the end EE installed=637692 failures=4123863;
6 MiB arena used=6286528, arena failures=4429867. The first allocation failures
appear in the 125978 ms report. Exclusive CPU sample totals: EE38.5%, IOP8.9%,
scheduler17.4%, compile30.6%, raster2.2%. These are software samples, not hardware
CPU/GPU utilisation. Sessions14–17 used EE Compact; they do not form a complete
comparable BIOS control. Arena counters persist across cold boots in one app.

## Implementation

- Nonallocating exact-size arena fit probe; no failure-count or epoch mutation.
- Arena epoch changes after successful allocation/release, including trim/coalesce.
- EE full-block, prefix and scalar-fragment compilation probe the actual emitted
  body size before final publication. A cached function keeps running without
  this probe; warm dispatch is unchanged.
- With less than256KiB headroom (or no fitting hole), clock-sweep cold EE owners,
  aiming at512KiB headroom. A first sweep is bounded at4096 metadata entries;
  a complete second sweep is only required if the requested body still cannot fit.
  Owners used in the previous256 block executions get a first-pass reprieve.
  This is a clock policy, not a global exact-LRU sort.
- Keep the full owner being replaced or receiving a prefix. No reclamation while
  a native chain executes. Release both code bodies, invalidate lookup/refusal
  hints and dispatch tags; serial checks reject old learned edges. IOP/VU/scalar
  cached allocations remain stationary and are not reclaimed by this sweep.
- If reclamation cannot admit the body, remember the arena epoch. Subsequent
  EE misses fall back without translating again until that epoch changes.
  This can conservatively defer a different smaller body too; correctness comes
  from the existing fallback, not an unchecked allocation. A changed guest opcode
  is never tagged unsupported because of arena exhaustion.
- Existing cached scalar functions still execute during deferral. No guest clocks,
  EE/IOP ratios, source/TLB checks, VU semantics or GX contracts are changed.
- All16K metadata owners, the6MiB executable arena and R1343 triangle culling stay.
  R1344 does not repair the source of collapsed VU/GIF geometry or claim playable
  game speed. No new ARM firmware or graphics API is involved.

New EE_CACHE_PRESSURE row: sweeps, released, deferred, scanned, blocked,
reclaimed_bytes. CODE_ARENA failures/high_water are app-lifetime counters; compare
new runs after fully restarting the app or compare counter deltas.

## Actual linked-PPC replay

Mocked SDK allocation/cache-maintenance; CPU and cache clients from the actual
Wii ELF. This selected exhaustion workload is not a BIOS timing/FPS estimate.

128 cold owners plus one hot owner and foreign code fill the arena. Run128 exact
eight-instruction grants at a new PC, same guest result in both builds:

| Measure | R1343 | R1344 |
| --- | ---: | ---: |
| Executed PPC instructions |15792984|526726|
| New failed precise translations |768|0|
| New installed precise bodies |1|1|

The old code eventually installs a smaller body after fallback; it still makes
768 failed attempts in the same replay. R1344 admits the needed full body once
and reuses it. The hot owner and foreign byte marker remain intact; retirement
and register result are exact. The96.7% reduction is confined to this stress test.

Additional tests cover: completely foreign-owned full arena (65 grants, one
translation, deferred retries, recovery on release); cached scalar code usable
during deferral; modified opcode fallback; prefix full-owner protection;
active-chain nested compilation refusal; complete teardown; probe side effects,
zero/overflow/full bounds. A native ASan/UBSan test covers12000 fragmented
probe/allocation/release operations and final full coalescing.

Final ELF passes35/35 focused linked-PPC regression groups plus34 prefix differential
grants, warm reuse and source mutation. Physical Wii FIRST_IMAGE/FPS still requires
a new owner run. Source: tools/verify_pressure_r1344.py,
tests/test_arena_probe_r1344.c, tools/verify_prefix_r1342.py.

## nullDC4Wii comparison

Inspected the previously downloaded pinned source26a623e3feca9927d8ffe43706b06d82d672dd64:

- dc/sh4/rec_v2/driver.cpp:6MiB arena, proactive cache clear at92% usage and hard
  floor128KiB free; recSh4_ClearCache resets block metadata and invalidates code.
- wii/dc/sh4/rec_v2/wii_driver.cpp: checks16KiB free before worst-case compilation;
  STATIC_GPR_ALLOC, pinned SH-4 GPRs and optional FPU pinning; native/link dispatch.
- dc/sh4/rec_v2/blockmanager.cpp: bounded buckets, address/code lookup mirror,
  frequency-based replacement and cache-mirror invalidation on replacement.

Their linear code arena uses reset; our allocator supports stationary owner
release/coalescing, so we can reclaim cold EE owners selectively. Neither design
makes more metadata a substitute for executable admission. SH-4 state contracts
and fault-driven fastmem cannot simply replace PS2 EE TLB/128-bit semantics.
The Wii PPC backend is custom, built on nullDC's rec_v2 framework, not a claim
that the entire emulator was written afresh. Enabling an option is not proof of
that path's benefit on every workload.

Source locations:
https://github.com/BenoitAdam/nullDC4Wii/blob/26a623e3feca9927d8ffe43706b06d82d672dd64/dc/sh4/rec_v2/driver.cpp
https://github.com/BenoitAdam/nullDC4Wii/blob/26a623e3feca9927d8ffe43706b06d82d672dd64/wii/dc/sh4/rec_v2/wii_driver.cpp
https://github.com/BenoitAdam/nullDC4Wii/blob/26a623e3feca9927d8ffe43706b06d82d672dd64/dc/sh4/rec_v2/blockmanager.cpp

## Physical Wii test

Fully exit to HBC; replace boot.dol and restart. Keep the same BIOS and GX/Fastmem
settings as session13. Block Cache Reuse ON; EE Compact OFF. COLD BOOT / NEW LOG.
BUILD must say R1344. Compare FIRST_IMAGE at the same stage and whether new arena
allocation failures remain zero; check EE_CACHE_PRESSURE reclaimed_bytes/released.
Counter growth in deferred identifies unavailable admission, not an opcode error.
ARM BIOS RAM jobs may be left OFF because the previous BIOS had zero jobs.
To compare Compact separately, cold boot with EE Compact ON; do not mix settings
inside a paused guest. No new performance option is needed for this fix.
