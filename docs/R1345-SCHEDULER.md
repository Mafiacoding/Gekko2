# R1345 Beta — shorter IOP entry with exact guest boundaries

## Physical evidence

Wii BIOS session18 is BUILD checkpoint=R1344, mask00fd7fff, GX enabled.
FIRST_IMAGE493721ms (8:14), versus R1343 session13 637131ms (10:37):
22.5% shorter elapsed first-image time. R1342 session12 was486423ms;
R1344 therefore recovers the regression, rather than establishing a new
large speedup over R1342. This is one owner run per build, not repeated trials.

Arena failures0 throughout; high-water6030048 of6291456 bytes;205 cold sweeps,
10211 owners released,6961952 bytes reclaimed. Final EE attempts3363354,
installed3363354,failures0. The remaining compile workload still matters.

Exclusive random CPU samples over session18: EE47.93%,IOP11.22%,
scheduler21.58%,compile11.36%,idle3.37%,GS raster4.54%.
These are sampled time categories, not hardware CPU/GPU utilisation.
After first image: EE47.14%,IOP10.72%,scheduler20.32%,compile12.02%,
idle3.79%,GS raster6.00%. Presentation is timed separately and must not
be inferred as free from zero sampled GX fields.

GX ready1,256 native draws,zero sync errors. All351232 Gouraud and33420 flat
triangles are degenerate; this release does not repair their VU/GIF source.
ARM available0,zero completed jobs; the unchanged worker does not provide
physical assistance. IPU command counts0 in this BIOS workload.

## Implementation

Host admission remains eight IOP quanta per outer call. Each quantum consists
of exactly eight EE scheduler slots followed by one IOP tick. The EE execution
callback remains inside the batch: IRQ,DMA,SIF and device deadlines retain their
existing boundaries. Idle/fault/HLE slots retain existing semantics.
The frontend max_slices cap is exact, including partial batches; input and
presentation return at the same frontend budget.

An experimental64-quantum host admission produced the same complete guest
signature but48099766 vs45854859 PPC instructions (+4.90%) in the14-grant
cold-cache/mailbox test. It was rejected and is absent from the delivered build.
Larger batches can change native-chain admission and compilation costs;
reduced outer-loop count alone is not proof of a speedup.

EE-led scheduling uses a specialised IOP one-tick entry. It retains the native
block admission, before-tick callback and scalar/recovery path of step_n(1),
without the general-budget loop. It does not replace native execution with an
interpreter. IOP halted returns0; a nonretiring idle tick still returns1.

Scheduler route switches are sampled once per frontend invocation. Settings
only apply at cold boot; stable EE/IOP objects refresh on every invocation.
EE Services OFF uses step_n(1) as the one-tick control.
The one-tick entry is used only in EE-led scheduling; IOP-led fallback retains
the general budget implementation. Existing R1344 arena fixes remain.

The EE INTC and DMAC pending probes read their canonical controller state
inline in Wii builds, eliminating a C call at retirement. There is no derived
pending cache: STAT/MASK, DMAC_CTRL.DMAE, enable byte and bus-error bit are
read live with the same predicates. MMIO, checkpoint and mutable getter writes
therefore remain immediately visible. Host builds retain the normal functions.
This mechanical inlining is always enabled in the Wii build; EE Services only
controls the specialised IOP scheduler entry, not IRQ semantics.

## Verification and limits

See verification files in the checkpoint. The IOP one-tick comparison covers
JIT on/off, idle on/off and halted on/off:128 complete IOP-state/RAM comparisons
per combination. In its active native example,146147 vs144195 PPC instructions
(1.34% fewer) for the selected128 calls. This is not total emulator throughput.
288 INTC/DMAC combinations check live Cause.IP2/IP3 through actual PPC parked
retirement, including disabled DMA, suspended DMA, masks and bus-error pending.
The scheduler differential test covers partial/full eight-quantum host batches, mailbox
ordering, exact EE retirement and IOP tick totals, with R1344 as baseline.

The selected512-call EE commit comparison produces identical complete EE-state
hashes and exact Count/retirement totals:110656 vs99904 PPC instructions,
9.72% fewer for this retirement path. It isolates scheduler work and does not
measure guest execution, compilation or physical Wii time. The combined11-grant
cold-cache/mailbox test changes15917028 to15884700 instructions (0.20% fewer);
cold compilation dominates that selected fixture. These scopes are not
interchangeable and neither is a promise of a doubled emulator speed.

No guest clocks are accelerated, no waiting CPU is skipped without an event
proof, no EE/IOP/VU state is merged, and no ARM worker changes are included.
A physical R1345 run is needed to measure FIRST_IMAGE and FPS.

## Owner test

Fully exit to HBC, replace boot.dol, restart. Use the same BIOS and settings
as session18: Cache Reuse ON, EE Compact OFF, EE Services ON; keep GX/Fastmem
unchanged. Cold Boot / New Log; wait through FIRST_IMAGE and the same later
BIOS stage. BUILD must read R1345. EE Services OFF is an optional separate
control run; change settings only before a new cold boot.
