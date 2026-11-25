# BETA12.33 — native GX work and experimental scheduler deadlines

19 October 2026. Branch `codex/beta12-33-gx-deadlines`, runtime build commit
`30c68e3`. This batch does not establish a BIOS-tile, Crash, Tekken or GT3 fix,
physical Wii speedup, or 30 FPS. Preserve BETA12.32 as rollback.

## Implemented

Native GX triangles no longer interpolate per-fragment depth in the permission
pass when depth comparison does not consume it. Equal vertex depths use their
exact common value in both permission and canonical Z-write passes. Varying
vertex depths still use the existing interpolation. Alpha/DATE/AFAIL decisions,
VRAM writes, readbacks, ordered completion tokens and 64-begin bounds remain.

The experimental EE-led scheduler now rechecks the IOP horizon at **every** EE
window admission, including the first and those following IOP catch-up. The old
code sampled the IOP limit only when entering an outer grant. An IOP event armed
by catch-up could consequently leave the next EE admission using an obsolete
limit. `ee_core_step_event_bounded_n` adds an external limiter without linking
EE directly to IOP; the existing three-argument EE API remains available.
External limits can shorten, never enlarge, the EE deadline, rounded down to
the existing eight-slot minimum. EE deadlines can grow again after an earlier
event has passed, within the original 64-slot ceiling (512 for the existing
idle path). No guest clock ratio changes or cycle-accuracy claim follow.

The existing `EE / IOP event batches` option remains factory OFF and applies
at cold boot. The normal scheduler remains the default/fallback. IOP catch-up
blocks, idle batching, long blocks, chain residency, PSM and GX risk retain
independent existing settings; this batch does not activate them automatically.
`width_sum` in the historical log is an outer requested-horizon accumulator,
not a measurement of each admitted window's actual width.

## Local BIOS reference result

The owner's unchanged BIOS was booted locally in official PCSX2 v2.8.2 and a
private GS command stream captured. Independent PCSX2 software replay of that
stream shows the blue cloud, shaded large cubes and Sony text **without** the
bright small squares in the supplied reference photograph. Feeding the same
captured GIF stream into Gekko2's software GS also lacks those bright squares.
This narrows the investigation: missing squares in a different screenshot do
not by themselves prove Gekko2 discarded their GS draw commands. BIOS/config,
NVM/card state and animation phase must be controlled before attributing the
difference to XGKICK, geometry, alpha or GX. No pixel-identity claim is made;
raw Gekko2 scanout is 640x256, PCSX2 presentation is 640x480, and photographed
screens differ in phase and exposure. Private replay tooling is exploratory,
not a complete published GS-dump compatibility implementation.

Local isolation attempts disabling the texel cache, running scalar EE, probing
AA1 alpha and VU0 macro flag handling did not restore the small squares. Those
experimental changes were **not** shipped. AA1 coverage and macro flag timing
remain separate audit topics. No generated squares, guest patches or global
brightness changes were introduced. Owner BIOS, NVM/cards, dumps, screenshots
and derived traces remain outside Git and release packages.

## Scheduler proposal: retain and develop, do not promote yet

| Proposal | Current implementation and limit |
| --- | --- |
| Replace fixed switches | Opt-in event batches already exist; this batch repairs renewal of IOP deadlines. Default remains the old path. |
| Dynamic EE/IOP windows | EE deadlines cap admission; owed IOP ticks can use existing optional native catch-up blocks. Enlarging either ceiling needs separate validation. |
| Preserve device ordering | Existing SIF register barriers, SIF DMA barriers, cross-core adapters, DMA notify and precise retirement remain. Full async device/VU ordering is not proven by these tests. |
| Provable idle skipping | Existing decoded VBlank-loop and parked-IOP paths have deadline guards. Other historical PC/hash heuristics are experimental and are not a proof of side-effect-free execution. No new heuristic is enabled. |
| Compare BIOS, Tekken, GT3 | Synthetic full-state comparisons are available now. Matched game progression and physical Wii comparisons are still open; no current game run is invented from old logs. |

The project still uses instruction slots as a clock approximation. A truly
cycle-driven queue requires consistent EE/IOP/device clock domains, explicit
next-event deadlines for deferred DMA/SIF/VU/IPU/CDVD work, deterministic tie
ordering, and differential tests at interrupts, shared accesses and wakeups.
Bigger budgets alone do not supply those semantics. XGKICK transports GIF
packets; it is not the geometry transform or a complete GS renderer.

## Where further Wii optimization has evidence

The complete second owner BETA12.32 session attributes sampled CPU work to GS
raster 23.85%, EE dispatch/scalar 23.56%, scheduler 20.02%, native attempts
18.02%, IOP 10.99%. These are sampled CPU shares, not whole-run utilization.
Recorded GX completion waits total about 2.127 seconds across 656.381 seconds
of PERF intervals; removing the now-working fences targets neither the main
measured CPU cost nor a demonstrated synchronization bottleneck.

The existing Wii Limits capture also distinguishes cache-sized and larger
working sets: dependent-load medians at 8 KiB are about 0.250 time-base ticks
per load in either memory pool; at 2 MiB they are about 4.041 in MEM1 and
12.412 in MEM2. Warm copy medians at 2 MiB are about 0.285 versus 0.870 ticks
per payload byte. These particular kernels support keeping hot metadata small,
contiguous and cache-friendly; they do not justify moving the entire guest RAM
into MEM1 or claiming the same multiplier for the emulator. Respect remaining
MEM1 headroom. No extra generic benchmark run is required for that conclusion.

Priorities are therefore fewer CPU passes over GX fragments (this batch),
less repeated EE dispatch/admission work, event batching with correct barriers,
and measured cache/locality improvements. Preserve IRQ checks and canonical
state. Use the existing profiler to attribute actual workloads, paired linked
PPC fixtures to reject regressions, and Dolphin for functional inspection.
Dolphin host speed is not the Wii's CPU/GX timing oracle. A targeted BIOS service
HLE can reduce its own work, but cannot remove GS/VU animation costs or establish
that a replacement BIOS has equivalent behavior.

## Validation

- All **83 host suites PASS** on the current source.
- All **21 linked PPC suites PASS**, including **105** interpreter/native
  residency full-state comparisons and **72** memory/HI/LO/SA/IRQ/TLB/SMC
  comparisons. SDK/cache/allocation services are modeled; guest execution
  and scheduler logic execute the actual linked ELF.
- Native GX synthetic work: **10,749,246 -> 9,058,418** executed PPC
  instructions (**15.73% fewer**); interpolation entries **17,160 -> 0** for
  this constant-Z/no-depth-test workload. Shader/configuration and vertex
  fingerprints match across all **1,529** cumulative submitted primitives.
  The **44** accepted native configurations remain guarded, four rejected
  configurations issue no submission, queue peak remains **64**, and the
  stale-token failure regression passes. This is not physical GPU pixel proof.
- Independent depth oracle: **512** linked PPC constant/varying-Z cases PASS.
  The general interpolation helper itself remains unchanged; the improvement
  comes from skipping calls at safe caller sites.
- Scheduler: **26** complete EE/IOP/timer comparisons, **6** SIF publication/
  echo cases, and **5** changing-deadline budgets PASS. The latter regression
  fails on the original BETA12.32 ELF at the missing deadline refresh.
  **12** parked/wakeup cases retain actual IOP progress without invented EE
  retirement. External deadline injection isolates renewal; it is not a
  complete device-timing simulation.
- Warm synthetic 256-IOP-tick scheduler work: legacy **932,290**, event-only
  **838,416** PPC instructions (~10.1% fewer). Event + existing long-block/idle
  options: **664,460**, a combined setting result, not attribution to batching
  alone. Cold/translation-inclusive work is worse here: at a 257-tick grant,
  legacy **4,339,108**, combined event options **8,862,395**. Compilation,
  changed block shapes and warm execution must therefore be measured separately.
  These are synthetic instruction counts, not measured cycles or BIOS/game FPS.
- Fresh devkitPPC r50/GCC16.1.0 + libogc3.1.0 chain and no-chain builds PASS
  DOL/ELF section identity and static MEM1 gates. Primary static end
  **0x815e5af4**, remaining **2,204,940 bytes**; no-chain remaining
  **2,341,644 bytes**. Dynamic heap/stack exhaustion is not excluded by a static
  gate. Existing SPEED/DSI/AOT/GX flags and safe completion fences are retained.

The historical all-files `tools/verify_regressions.py` was also attempted and
stopped at compilation because its flags omit the libchdr include directory
(`libchdr/chd.h` missing). It is not the current release gate and was not
repaired in this batch. The successful 83-suite invocation is
`python3 tools/test_beta12_31_host.py --output-json REPORTS/host.json`, using
`tools/build_host_core.py` for the configured host build.

Verification records and exact binary hashes accompany the review package.
Host, linked PPC with modeled SDK services, static Wii build gates and physical
Wii observations are separate evidence categories. New physical Wii/game
comparison is still pending. See the final manifest for suite counts and hashes.

Reference implementations inspected: upstream [GSState](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/GS/GSState.cpp),
[GS dump/replay format](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/GS/GSDump.cpp),
[software scanline renderer](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/GS/Renderers/SW/GSDrawScanline.cpp)
and [VU operations](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/VUops.cpp).
The local runtime reference is the official v2.8.2 Linux Qt release. No BIOS,
private GS stream or upstream binary is redistributed with this batch.
