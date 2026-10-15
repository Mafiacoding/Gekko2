# R1346 Beta — bounded EE/IOP host plans and VU native re-entry

## Scope and guest contracts

The existing CPU inactive service guards option enables this release's host
plan grouping and VU re-entry. It is already enabled by default and in Wii
BIOS session18. VU re-entry additionally requires VU dynarec ON. No settings
format change or silent guest clock acceleration is introduced.

The host EE/IOP plan admits64 IOP quanta instead of8. Each IOP quantum still
has exactly eight EE scheduler slots, including idle/fault/HLE slots, followed
by one IOP tick. The callback still runs inside native execution boundaries;
IRQ,DMA,SIF and memory effects are not delayed until the outer plan ends.
The exact frontend max_slices cap remains, including partial plans and halted
cores. The IOP one-tick route and live IRQ probes from R1345 remain.

A critical difference from the rejected R1345 experiment: each call to
EE step_n is capped at64 slots within the larger plan. Native chain admission
and compilation therefore cannot consume the entire512-slot host plan.
This separates host administration from the native execution horizon.
No larger code arena, cache-owner relocation, duplicate source ownership or
unchecked TLB assumptions are introduced. R1344 pressure handling remains.

VU0/VU1 previously selected either the pair or block runner once at entry.
A branch/delay/stop/Q/P condition at that entry could select the pair runner
for the entire microprogram, even after the condition cleared. Both units now
use the existing guarded block runner when the option is enabled. It checks
admission again after scalar boundary work, allowing later arithmetic to
resume native blocks. Branch delays,E/D/T flags,Q/P publication,WAITQ/WAITP,
XTOP/ITOP,XGKICK and hazards retain the existing one-pair boundary scheduler.
Native pairs advance the existing pipeline clock by their exact retired count.
The existing65536-pair guard and MSCNT continuation semantics remain.

VU execution is still synchronous within the project's existing model;
this does not implement a new asynchronous PS2 timing model or a merged chip.
Registers,memory and instruction sets remain separate. ARM firmware is unchanged.
GX is still driven by GIF packets; this does not repair collapsed vertex data.

The32-entry VU block cache also folds pair-granularity PC bits into its
existing hash: (pc>>3)^(pc>>6)^(pc>>11)^owner-salt. Previously two short
segments in the same64-byte window, especially around XGKICK, could replace
each other on every run. Exact owner,PC,mask and source-word validation remains.
The warm VU cases assert zero block retranslation after priming.

New log row:
UNIT_PLAN host_quanta=64 native_horizon=64 ee_slots_per_iop=8 vu_reentry=1 vu_execution=synchronous

CPU inactive service guards OFF restores the small host plan and entry-selected
VU runners as a comparison path, alongside its pre-existing service behaviour.
This switch changes several optimisations, so use the R1345 build for a cleaner
whole-release control. VU dynarec OFF retains the interpreter/pair route.

## Verification

The linked-PPC EE/IOP/SIF test covers11 partial/full grants through65 ticks.
R1345 and R1346 yield identical complete EE/IOP-state hashes, exact retirement
and tick totals and mailbox echo ordering. Selected cold-cache cost:
R1345 15884700 vsR1346 15882741 PPC instructions (0.012% fewer).
This small fixture does not establish a meaningful physical FPS gain.

The VU test compares complete state and local memory across control,bundled
and interpreter routes for both VUs, branch,Q,P and E/MSCNT continuation cases.
39 route cases,each with four warm runs,pass. Selected warm branch-first
32-pair arithmetic programs: VU0 51464->16456 instructions (68.1% fewer),
VU1 46204->15960 (65.5% fewer). Short branch-first programs also improve.
The XGKICK cases additionally compare exact GIF-consumer packet bytes,
TPC,retirement and pipeline clock at each handoff, including wrapped VU1
local memory and the absence of a VU0 GIF connection. GIF consumption is mocked,
not actual GX rendering. Selected warm VU1 XGKICK:47112->19600 instructions;
wrapped XGKICK:92380->64864.

Q/P-start cases have much smaller gains because the old candidate already
admitted their guarded block runner. These are workload-specific PPC costs,
not predictions of total Wii FPS or BIOS boot time.

The complete final binary must pass35 linked-PPC regression groups before
packaging. Verification logs are included in the checkpoint. Hardware execution
of R1346 is not yet measured; session18 belongs to R1344.

## Test on Wii

Fully exit to HBC, replace boot.dol,restart. Keep session18 BIOS,GX,Fastmem,
Cache Reuse ON and EE Compact OFF. Keep CPU inactive service guards and VU
dynarec ON. Cold Boot/New Log; BUILD must read R1346 and UNIT_PLAN must show
host_quanta64/native_horizon64/vu_reentry1. Compare the same BIOS stage, then
Tekken independently; include FIRST_IMAGE,PERF,TIME_SAMPLE,VU_JIT and cache rows.
