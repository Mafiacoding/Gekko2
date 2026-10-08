# R1333 — Cold Boot, session logs and budget-aware EE cache

Updated 2026-10-08. Physical Wii is the primary target. This checkpoint adds
repeatable boot sessions and a measured cache optimization; it does not certify
an OSDSYS boot fix or playable performance.

## Controls and option application

| Action | Wii Remote | GameCube |
| --- | --- | --- |
| Pause emulation / resume existing session | PLUS | START |
| Guest PS2 START | MINUS+A | Z+A |
| Toggle diagnostic HUD while running | MINUS+PLUS | Z+START |
| Fresh boot with new log | COLD BOOT / NEW LOG, A | COLD BOOT / NEW LOG, A |

HOME also pauses on Wii Remote. HOME+MINUS and the menu Exit action return to
HBC. The inherited GameCube B+Z pause and B+Z+START exit actions remain.
Guest START chords suppress guest Cross/Select. Guest Select remains MINUS.
In the disc browser use B to go up/back; PLUS resumes the existing session.
START/PLUS with no initialized session only shows a notice.

BIOS / OSDSYS and BOOT DISC start fresh sessions. COLD BOOT / NEW LOG restarts
the current BIOS/disc mode using the selected disc image, if applicable. It
requires no application exit. A restart resolves old GPU-owned VRAM before
unmounting discs and releasing EE/IOP memory; a failed resolve retains the
paused session and reports an error.

CPU optimization settings and the master GX setting apply only at the next
cold boot. Resume keeps the active code, RAM, mounted disc and GX setting.
EVENT records distinguish active_mask/GX from next_boot_mask/next_GX. Launcher
FPS/HUD presentation preferences remain separate. The Optimized build defaults
Block cache reuse ON; Control defaults it OFF. Saved v2 settings override these
defaults. Both builds retain the same renderer, 6 MiB arena and tests. Fastmem
remains experimental and default OFF; it is not a GX feature.

## Logs

A boot that successfully loads the BIOS reserves a file before core setup:

`sd:/pcsx2/logs/Gekko2-R1333-0000000001-bios-gx.log`

Names encode checkpoint, monotonically numbered session, BIOS/disc and initial
GX/software mode. Existing names are scanned across modes and application
restarts. Exclusive creation preserves previous and partly written files.
Pause/resume appends to the same file. A later cold boot gets a new file.
Startup diagnostics are still separately written to Gekko2-startup.log.
BIOS load failure before session initialization is reported by startup/UI
handling rather than a reserved boot log.

SESSION, BUILD and EVENT records identify the build, session and transition.
PROGRESS, FIRST_IMAGE, PAUSED and earlier state diagnostics remain. Pending
performance intervals are flushed on pause/stop as well as the periodic sample.
TIME_SAMPLE, TIME_PRESENT, HOST_TIME, BLOCK_CACHE, IOP_ROUTES and GS counters
remain available; EE_CACHE_BUDGET adds fit_hits, budget_misses and
variant_installs. LOG_STATUS includes path, cumulative errors and last errno.
Open, buffered write and close errors are detected and surfaced by diagnostics.
Emulation can continue when a log cannot be written. This is checked stdio
flush/close, not a guarantee against SD removal or power loss.

Boot-local EE one-shot flags, three EE SIF system registers, selected Fastmem,
GIF/render and dynarec translation/residency counters now reset. Existing core
initializers continue their own resets. This is not a claim that every legacy
trace counter has been converted. Arena high-water/failure counters remain
process-lifetime. Hardware GX/FIFO buffers are retained; ready=1 alone proves
neither native draws nor BIOS progress. Resume does not reset execution/cache
counters; sampled profiler intervals restart and wall ms includes paused time.

## Cache optimization

With Block cache reuse ON, the four-way precise EE cache keeps different-length
variants at the same PC. Lookup selects the largest compiled variant fitting
the current retirement budget. A shorter budget can install a second variant
without discarding a reusable long block. Native successor lookup uses the
remaining budget too. OFF retains the previous single-way lookup policy.

Live source/mapping checks, generation/ownership guards, pinning, retirement,
interrupts, Count/Compare and the EE8/IOP1 interleave remain. Code arena size,
cache index hashing and scheduler quantum are unchanged. No directly patched
branch or persistent cross-block register scheme is claimed here.

Synthetic alternating budgets [8,2] repeated 16 times:

| Metric | Reuse OFF | Reuse ON |
| --- | ---: | ---: |
| Installed EE translations | 3 | 2 |
| Cache lookups / hits / misses | 33 / 30 / 3 | 32 / 30 / 2 |
| Executed modeled PPC instructions | 145,993 | 119,273 |
| Retired guest instructions / register result | 160 / 160 | 160 / 160 |

This is an approximately 18.3% instruction reduction in one artificial replay,
not Wii cycles, BIOS timing or FPS. Guest source mutation is verified across
both length variants. Real profiler logs are needed to establish benefit at
the owner's workload and whether other stalls dominate.

## Validation and first hardware test

- 25/25 linked PPC suites using the Optimized ELF, including EE/IOP/VU,
  Fastmem, source/TLB changes, scheduling, helper ABI, renderer routing and cache.
- New budget/control/reset suite also passes on the Control ELF.
- 64/64 native GS tests using current portable renderer sources.
- Three same-process host cold boots reset tested flags/registers and execute
  the synthetic EELOAD one-shot again; no copyrighted BIOS is used.
- Log tests preserve old files, append pause/resume, survive fresh logger
  instances and detect buffered write failure; ASan/UBSan pass.
- Both DOLs match ELF section bytes, entry 0x80004000, Wii HID4 markers and
  MEM1 bounds. These are structural checks, not physical-Wii boot tests.
- Launcher preview is a host render of the actual drawing routines.

Install Optimized as boot.dol. Check Block cache reuse is ON because saved
settings override its default. Keep a known test setup, choose COLD BOOT / NEW
LOG, and verify BUILD checkpoint=R1333 plus SESSION and active option flags.
Pause with PLUS, resume with PLUS: the same session/path and guest state should
continue. Pause and choose COLD BOOT: a second file should start without
replacing the first. Compare BIOS and Tekken separately; send the numbered
session files rather than the old R1308-named log. Change one relevant option
between otherwise matching cold runs for a useful comparison.

Inherited native IOP RAM/COP0 paths, EE Fastmem and GX resident paths remain.
Full EE/VU coverage, broad Gouraud depth/alpha rendering, persistent FPU pinning,
direct patched links and fault-driven PPC-MMU Fastmem remain incomplete.
