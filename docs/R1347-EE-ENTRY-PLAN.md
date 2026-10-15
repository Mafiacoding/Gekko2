# R1347 Beta: admitted EE entry within the bounded unit plan

## Change

CPU inactive service guards ON now admits the first EE instruction before
cache selection, prefix allocation or PPC translation. Already fetched
instructions reuse the scalar fetch. Other entries use the existing pure
source peek. Memory operations must prove their current direct-RAM address
with the existing alignment, TLB V/D and range checks before translation.
A decline leaves guest state untouched and returns to the scalar frontend.
Thus a temporary MMIO, missing TLB entry or unaligned first memory operation
cannot allocate a new body and immediately discard it. A warmed owner also
survives such admission declines. Admission does not cache a data address
across calls: every entry resolves its current address.

After compilation/cache lookup, the normal prepare_fetched callback performs
the pipeline transition. The emitted first instruction consumes the proven
fetch and RAM address rather than repeating its fetch/address preparation.
Warm lookup compares the admitted opcode even for unfetched entry. Raw first
word mutation therefore cannot remain stuck behind a retained stale owner.
Later instructions retain their existing live code and address guards.
A nonsequential next_pc now rejects entry before compilation in both routes.

The R1346 bounded scheduler plan remains: host_quanta64, native_horizon64,
eight EE slots then one IOP tick. IRQ/DMA/SIF callbacks stay at their original
boundaries. No deferred retirement, guest clock acceleration, register state
fusion, new ARM firmware or geometry repair is introduced. Cache pressure
handling and VU re-entry remain unchanged. This release combines admission
with translation/execution inside that existing scheduler plan; it does not
merge the guest processors or guarantee a twofold improvement.

UNIT_PLAN adds ee_entry_admission=1 when the service option is enabled.
The existing settings format and defaults are unchanged. Turning the service
option OFF also changes the prior host plan/VU/service optimisations; use the
R1346 binary with the same settings for a cleaner release comparison.

## Verification

The focused actual linked-PPC entry test checks 14 cases: MMIO, unaligned
and missing-TLB declines without guest mutation; cold translation avoidance;
warm owner retention; recovery to RAM; first-word mutation without notification;
nonsequential pipeline rejection; and complete warm arithmetic state equality.
For 100 warmed eight-instruction arithmetic blocks, service OFF uses276142
PPC instructions and admission ON uses267442 (3.15% fewer). This isolated
entry result does not predict physical Wii FPS or BIOS boot time. The frontend
already fetches many entries, so the savings depend on the actual workload.
The 11-grant cold EE/IOP/SIF fixture has the identical full guest-state
signature on R1346 and R1347. PPC cost falls from15882741 to2676708
(83.15% fewer). This MMIO-heavy fixture includes cold translation work and
strongly favours the removed admission/translation churn. It is not a
representative BIOS or game benchmark and does not imply an83% time reduction.
The full linked-PPC regression groups and scheduler, IRQ, prefix, pressure
and VU checks must pass on the frozen final ELF before packaging. Their raw
logs are supplied with the checkpoint. Platform services are mocked in these
checks; physical Wii/ARM/GX execution is not established by them.

## Wii test

Exit completely to HBC and replace boot.dol. Keep the same BIOS and settings
as the R1346 control: Cache Reuse ON, EE Compact OFF, CPU inactive service
guards ON. Keep GX, Fastmem, HLE and all other settings identical between
builds. Use Cold Boot/New Log. Confirm BUILD R1347 and UNIT_PLAN admission1.
Compare FIRST_IMAGE time and EE_CACHE hit/miss/stale/installed values at a
similar EE count, plus PERF/TIME_SAMPLE. Test Tekken separately with a new log.
An increase in EE count alone is not a successful boot or speedup measurement.
