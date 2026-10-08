# R1332 — cache diagnostics, runtime profiler and IOP RAM paths

Baseline: R1331, owner Wii tests dated 8 October 2026. Wii is the primary target.
This checkpoint preserves the renderer and the existing 8:1 EE/IOP schedule.
No physical Wii BIOS boot, gameplay or FPS improvement is claimed here.

## Changes

- The new cold-boot option **Block cache reuse** gives the EE cache 256 sets ×
  four ways (1024 owners), and the IOP cache 128 sets × four ways (512 owners).
  OFF uses one way, retaining the old direct-mapped behavior. Colliding PCs
  can coexist. Full sets replace owners round-robin, after native execution
  returns. Learned EE links retain pointer, index, serial and generation
  validation; reset releases every way. This is not directly patched linking.
- With cache reuse ON, IOP block formation emits at most the current budget,
  rather than repeatedly generating up to eight instructions for a one-tick
  grant. Each tick still executes the original preparation/HLE, load-delay,
  retirement and interrupt checks. We do **not** enlarge the IOP grant or
  defer EE/SIF interactions just to make a block counter look better.
- **Fastmem RAM page cache** retains R1331's positive EE RAM/TLB cache and
  native EE resolved-RAM instructions. It now also selects guarded direct PPC
  byte/halfword/word IOP loads/stores. Generated PPC checks live RAM pointer,
  bounds, address policy, alignment and store IsC. RAM hits avoid the data
  helper; MMIO, ROM, failed proofs, cache-isolated stores and alignment cases
  keep the existing helpers. IOP C helpers additionally check bounded RAM
  before walking device handlers. Merge loads/stores retain their helpers.
  Fastmem remains default OFF, experimental, with no PPC-MMU/DSI mapping.
- IOP MFC0/MTC0 register transfers now have native PPC bodies. The original
  pipeline preserves MFC0 load delay; retirement still delivers pending IRQs.
  Unsupported instruction bodies, HLE, exceptions and device services remain
  explicit C paths. We do not call these services a completed native IOP.
- The options file is version 2, with 14 options. Version 1 files are accepted
  and gain only this build's default for the newly added cache option. Version
  2 files preserve the exact requested mask. Changes apply on cold boot;
  resume preserves the current CPU code-generation mode.

## Builds and first Wii comparison

**Gekko2-R1332-Optimized.dol**: cache reuse defaults ON.
**Gekko2-R1332-Control.dol**: cache reuse defaults OFF.
Both have the same code, profiler, IOP coverage, renderer and 6 MiB arena.
EE native blocks default ON in both; Fastmem and the GX master switch default
OFF. Both read the same saved options file. A saved v2 selection overrides
these defaults: check the option or the active BUILD mask rather than trusting
which filename was launched. Control is not the earlier arena-free Reference.

1. Copy the Optimized DOL as `sd:/apps/Gekko2/boot.dol` (the package already
   contains that layout). Keep the supplied meta/icon alongside it.
2. Settings → Down → CPU / GX Optimizations: **Block cache reuse ON**,
   EE/IOP/VU dynarec ON, EE native blocks ON. Start with **Fastmem OFF** and
   ordinary **GX OFF**. Preserve other settings for comparisons.
3. Cold-start BIOS / OSDSYS with A. START resumes instead of applying changes.
4. Save the log before another cold boot (the first progress record overwrites
   the old log). Compare OFF/ON cache reuse and then OFF/ON Fastmem separately,
   with the same BIOS, GX mode, EE interval and idle/active state.
5. Only after a first BIOS picture is recognized, compare GX ON separately.
   `requested=1` and `ready=1` alone do not prove native graphics draws.

Legacy filenames remain `sd:/pcsx2/Gekko2-R1308-software.log` and
`sd:/pcsx2/Gekko2-R1308-gx-render.log`; startup is `Gekko2-startup.log`.
The runtime BUILD record identifies R1332 and its active/next-boot masks.

## Log interpretation

| Record | Meaning |
|---|---|
| BLOCK_CACHE cpu=EE/IOP | Cumulative lookups, hits, misses, collisions, stale entries, translation attempts, installed blocks and failures |
| compile_sample_tb / compile_samples | Translation duration for one in 64 attempts; divide deltas to get sampled mean TB ticks, not total execution time |
| TIME_SAMPLE | Exclusive host TB costs in randomized EE/IOP scheduler samples: EE, IOP, scheduler/boundaries, idle, compilation, GS raster, texture packing, readback and GX wait |
| TIME_PRESENT | Separate fully timed frontend/presentation intervals, with nested upload/readback/wait costs removed from the frontend remainder |
| HOST_TIME | Full core and presentation durations plus SD boot/performance log writing; current performance-write cost enters the following interval |
| IOP_ROUTES | Executed native bodies, interpreter bodies, preparation-recovery ticks and C RAM-fast-path access counts |
| FASTMEM | EE translation cache hits, slow translations and invalidations; not a count of every direct native RAM access |
| CODE_ARENA | Shared executable memory occupancy, high-water and allocation failures |

TIME_SAMPLE deliberately samples randomized grants (128..383 grants apart),
not every instruction. Nested scopes charge time once. `scheduler` includes
native EE admission/retirement boundaries and IOP tick/device service; HLE
work not covered by a specific nested scope stays in its CPU remainder.
`idle` covers explicit parked service, not every guest polling loop.
GS_raster includes the remaining GIF raster work after separately scoped
packing/readback/wait is removed. GX_upload measures texture packing rather
than every cache flush or FIFO instruction. Broader frontend setup, HUD,
VSync wait and CPU framebuffer output stay in the presentation remainder.

Sample totals and exact presentation/log totals have different coverage: do
not add them together as if all were full-wall-time buckets. Sparse expensive
operations can skew a short sample; compare multiple intervals. Profiling and
route counters themselves have overhead. TB wrap is handled modulo 32 bits;
individual scopes must finish within one TB wrap. The controlled-clock tests
verify accounting, not the actual physical GPU or Wii clock speed.

A block call/tick is not automatically a native instruction: a block may
recover through HLE or the scalar core. `IOP_ROUTES` supplies the distinction.
The helper fast-read/write counts exclude direct PPC accesses that bypass the
helper. Output-FPS count presentations; guest VBlank is a separate counter.

## Owner log findings and test scope

The supplied R1331 GX/Fastmem log ends at 266,352,180 EE with first=0 and zero
GX draws. The supplied software log recognizes RGB at 295,414,371 EE after
1,736,874 ms. These runs do not establish a Fastmem boot regression: the GX
run ended before that first-picture milestone. The separately supplied
startup log says R1330-L; the two performance logs identify R1331 internally.
They are not proof that all three files belong to the same launch.

The original compile-time `WORD_ALLOC bodies` counter reached 618,369,898,
while final arena high-water was 547,488 bytes with zero failures. These are
strong reasons to measure retranslation and collision cost, not proof that
all core time is spent compiling. The new counters distinguish those costs.

The artificial four-PC collision replay checks identical guest results,
reduced translation counts (EE 32→4, IOP 64→4), live source mutation and
lower modeled PPC instruction cost. It deliberately stresses one set and
cannot predict a general BIOS FPS factor. The IOP Fastmem suite compares
complete CPU/pipeline state plus observed RAM with OFF/ON, checks native
execution of COP0 transfers, and verifies that direct data LW skips its
helper. It covers MMIO, IsC, bounds, aliases, alignment and load delay.

The archive's Verification.json and logs give authoritative shipped-build
results. Full linked-PPC CPU/scheduler/ABI and GS-routing checks execute the
ELF with modeled allocator, libc memset and libogc/EFB services. Native GS
checks execute portable current sources. No BIOS/disc/runtime memory/logs are
included. Hardware profiling remains the next source of evidence.
