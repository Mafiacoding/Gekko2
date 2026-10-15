# R1338 — EE cache retention and explicit ARM startup IOS

Physical Wii remains the primary target. No measured R1338 Wii boot/FPS or
physical ARM load/throughput result is available yet. R1337 decoder, stable
six-MiB arena and asynchronous CSC worker remain; no BIOS/game/SDK is included.

## EE translation cache

Block cache reuse ON now replaces the least recently used owner in each
four-way EE set. Successful frontend dispatch and guarded native successors
refresh recency. Metadata stays separate from the Broadway dispatch cache line;
published code never moves. Wrap resets recency metadata, not executable owners.
Control/reuse OFF retains the direct-mapped policy. Compact mode remains optional.

When the set is full and a long variant exists at the same PC but cannot fit
the current scheduler grant, a new short variant is declined. An empty way may
still admit a variant. The existing scalar PPC JIT handles the remaining work;
unsupported instructions keep interpreter fallback. Subsequent PCs can still
cache their own blocks. This prevents budget-tail variants from displacing four
already occupied owners. It does not eliminate every translation or variant.
EE_CACHE_BUDGET deferred_variants counts these declined admissions.

No grant is enlarged, instruction skipped or guest progress injected. Per-word
source/mapping checks, serial-guarded successors, precise IRQ/Count/timers,
delay slots and EE/IOP interleave remain. Executing owners cannot be freed.
The allocator/code arena stays PPC-addressable RAM; ARM cannot execute PPC code
or remove source invalidation and PPC instruction-cache maintenance.

## Synthetic linked-PPC comparison

Same workload and guest results, baseline R1337-Beta-ARM versus R1338:

| Workload | Baseline translations | R1338 translations | Baseline PPC instructions | R1338 PPC instructions |
| --- | ---: | ---: | ---: | ---: |
| Three hot PCs, two alternating cold PCs in one set | 30 | 15 | 1119680 | 647432 |
| Short grants interspersed with full hot-block grants | 20 | 16 | 3618001 | 3357776 |

The first case halves installed translations and uses about 42% fewer modeled
PPC instructions; the second uses about 7% fewer. These are targeted artificial
replays, not physical-Wii cycles, BIOS times or universal speedups. Admission
can incur extra scalar dispatch work; inspect actual workload profiler logs.
Live instruction mutation is checked after admission refusal too.

## ARM persistence and IOS selection

The previous beta loaded requested options but inspected the still-default
active mask before startup IOS selection. Saved ARM ON therefore failed to
select IOS222. R1338 applies loaded settings before checking startup IOS.

Options include independent ARM IPU worker ON/OFF and ARM startup IOS
CURRENT/IOS222. Default is CURRENT. Older configuration versions remain readable;
old v3 ARM ON does not silently select IOS222. Select both settings explicitly.
The option page shows the actual active IOS and a readable selection result.
Changes to ARM/IOS require exit to HBC and application restart. CPU/GX changes
still apply on cold boot; pause/resume retains the running session.

Saved v4 settings support existing-destination FAT-style rename, checked close,
backup/rollback on replacement failure, and missing-primary backup recovery.
A reported save failure is not success; SD removal/power loss is not guaranteed
safe. Older releases cannot read v4 configs; reselect the same options manually
when comparing an older build, retaining a copy of its config if needed.
Startup log records load_status, requested/active masks and ARM_IOS selection.

Copy arm/Gekko2-ARM-Worker.elf to sd:/pcsx2/arm/Gekko2-ARM-Worker.elf.
Enable ARM and select IOS222, save, exit to HBC, relaunch, then cold boot.
IOS222 must already be installed, non-stub, and supply compatible /dev/mload.
The supplied earlier SysCheck recorded IOS222 rev65280 (stub). R1338 rejects
missing/stub IOS222 and keeps the current IOS. It does not install/patch IOS or
write NAND. Current IOS is valid if it already supplies the required service.

After successful selection, the existing bounded MLOAD loader validates and
loads the worker into authorized unused RAM and checks /dev/gekko2 capabilities.
ARM_IOS status=1 means MLOAD found, ARM_LOADER status=1 means thread started;
ARM_WORKER available=1 plus submitted/completed demonstrates actual jobs.
The worker offloads CSC only, not EE/IOP/VU, IDCT or translation cache execution.
The supplied BIOS logs issue no CSC jobs, so this worker cannot accelerate that
boot phase merely by being enabled. Hardware service loading remains unverified.

## Validation and Wii test

Run tools/verify_r1338.py against the final ELF for linked CPU/GX/IPU/cache/IOS
checks. Native tests include fresh-process ARM/IOS save/load, simulated FAT
replacement, rollback/backup recovery and eight IOS/SD/controller lifecycle
cases. Platform/IOS transport is mocked; guest PPC/ARM code is executed.
See packaged Verification.json and logs for actual results and limits.

First use matching known settings, cache reuse ON, compact OFF and ARM OFF;
cold boot/new log. Compare FIRST_IMAGE ms, compile_sample_tb, attempts,
variant_installs/deferred_variants and collisions at the same visible phase.
Then independently test ARM startup and inspect actual IOS/service counters.
No KOF/Tekken title-screen or physical R1338 performance result is claimed.
