# R1354 Beta — claimed SIF barriers and sampled-time correction

R1353 Wii logs30/31 reached first image at383667/384217ms, compared with
R1352 log29 at418915ms. This is first-image latency only, not sustained speed.
Both new logs ran event batching and long blocks; SIO2 HLE had no registered
exports/calls. ARM handshake succeeded but no jobs ran. Idle batches skipped
only28/36 ticks. Barrier calls are not IOP ticks. Do not interpret 48.3M calls
as 48.3M synchronized ticks or sampled idle as proof of idle-loop skipping.

## Change

EE memory access probes multiple device handlers. R1353 synchronized at the
entry to sif_mmio_read32/write32, including rejected addresses. R1354 first
checks the seven claimed aligned addresses1000f200..1000f260. Rejected probes
leave the output unchanged and never catch up or mark the EE window urgent.
Actual SIF register accesses retain synchronization before reading/mutation.
SIF-DMA and IOP RAM bridges retain their previous synchronization behavior.
No changes to the IRQ, timer, guest Count, clock ratio or idle rules in the
barrier fix. Optional registered RAM HLE has the existing one-tick HLE timing
model rather than instruction-exact function timing.

SYNC_REASONS counts active EE barrier calls, in this register order:
MSCOM,SMCOM,MSFLAG,SMFLAG,CTRL,F250,F260. Read and write vectors are separate.
Additional fields distinguish SIF DMA reads/writes, IOP RAM bridge reads/writes,
and other callers. Recursive IOP calls and calls outside an active EE event
batch do not count. IRQ/timer urgent notifications are not shared barriers and
are not mislabeled as such. Sum the reason counters to recover shared_barriers.

The sampled EE/IOP timer previously started a new sample after the last IOP
catch-up and sometimes closed it with an old IOP-middle timestamp. Unsigned
subtraction then produced implausible CPU_SAMPLE EE totals. R1354 explicitly
tracks whether an IOP middle exists and assigns an EE-only tail to EE time;
it also prevents replacing an unfinished sample. These samples remain sampled
EE/IOP cost ratios, not total CPU utilization. TIME_SAMPLE is separate.

## Verification

- Actual linked PPC:30 claimed/rejected SIF read/write and reason checks.
  The rejected-address regression fails on R1353 at address0 and passes here.
  Active SIF read/write reason counters sum exactly to the barrier counter.
- Seven forced-sampling event runs: bounded sampled time and exact EE/IOP
  retirement ratio, including end-of-call tails.
-26 complete EE/IOP/timer comparisons against8:1 interleave and six real SIF
  publication/echo grants. Guarded DSI full-state, fault/backpatch/ownership,
  and handler restoration tests pass.
- Host handshake and30000 EE plus30000 IOP timer transactions pass.
- DOL section bytes match ELF, Wii entry80003f00 and valid MEM1 bounds.

Synthetic warm loop,256 IOP ticks/2048 EE instructions after512 warm-up ticks:
INTC reads: R1353=2242820 PPC instructions,512 false SIF barriers,256 catchups;
R1354=2092331 PPC instructions,0 false barriers,88 catchups. Full guest-state
hashes match. This is6.7% fewer synthetic PPC instructions, not a Wii FPS claim.
A RAM-store loop already using the guarded fast RAM path has0 barriers in both
builds and essentially unchanged cost(1513138 vs1513150). No universal gain.
Pure arithmetic scheduler cost is essentially unchanged from R1353.

The older verify_ppc_timers_r1281.py asserts eager writes to a private clock
counter and fails on the inherited deferred-timer implementation. It is not
used as a pass claim; the deadline/probe-aware timer tests above are used.

## Additional CPU / HLE options

Two new OFF-by-default options apply on Cold Boot:
- IOP catch-up native blocks: execute the owed tick budget with existing1..8
  instruction native IOP blocks, retaining per-instruction source, timer, IRQ,
  exception and load/branch-delay checks. The ratio and owed ticks do not change.
- IOP adaptive block cache: expand a warmed, nonterminal owner when its grant
  grows. Formation limits prevent repeated expansion after a source boundary;
  control-transfer owners do not grow across their delay slot. Replacement
  occurs only outside active execution, preserving source and arena ownership.

Version8 stores these two bits29/30. Version6/7 settings preserve earlier bits
and leave new bits OFF; older formats reject masks containing new bits.

The existing registered service HLE option now also recognizes sysclib exports
12(memcpy),13(memmove),14(memset). This requires real module-loader registration,
unchanged function bytes and the HLE RAM option. Only proved normal IOP RAM,
up to64KiB, is handled. MMIO, invalid segments/ranges, oversized work, overlapping
memcpy, pipeline hazards and self-modifying function targets retain guest code.
memmove supports overlap. Return pointer, RA continuation and zero register are
preserved. No ROM-wide search, forced module registration or full BIOS HLE.
IOP_RAM_SERVICE logs actual calls, bytes and range declines.
If ARM BIOS RAM jobs is ON, an eligible nonoverlapping64KiB RAM job can try the
existing verified ARM copy/fill service; failures and overlap retain CPU work.
This is synchronous data assistance, not IOP/EE execution on ARM. Worker unchanged.

72 sysclib RAM ABI/alias/overlap/bounds cases and disabled/source-changed fallbacks
pass. Six registered64KiB ARM copy/fill service cases verify success and
complete CPU fallback after partial transport failures. SIO2 and sysclib coexist across re-registration(20 distinct services).
21 mixed-budget IOP states agree with scalar execution, including1->2->4->8
owner growth and source replacement. The existing linked477-program IOP suite
passes source replacement, active-code pinning and ROM HLE guards.

## Wii test

Use the same R1353 log30 settings, with Event scheduler, IOP idle batching and
EE long blocks enabled, SIO2 service HLE disabled. Keep GX/Fastmem/DSI and other
settings identical for the comparison. Cold Boot after changing settings.
Test1 uses the same log30 settings, with both new IOP options OFF.
Test2 enables IOP catch-up native blocks plus IOP adaptive block cache, Cold Boot.
Keep registered service HLE OFF for this pair. Test3 can try registered service
HLE and HLE RAM bulk operations with Tekken; check actual calls/bytes instead of
assuming enabled means used. ARM work still requires a real eligible request.
Stop after the first BIOS image plus30..60 seconds; send the new log.
Confirm BUILD R1354, inspect SYNC_REASONS and CPU_SAMPLE, and compare first-image
time and guest_vblank/EE_per_s at comparable guest progress.
Keep the existing working R1352/R1353 ARM worker; protocol and worker unchanged.
No new ARM CPU offload, geometry correction, busy-loop recognition, full IOP HLE,
full JIT, quick-boot checkpoint or promised twofold speedup in this focused beta.

Build with devkitPPC r50/GCC16.1.0, libogc3.1.0, same SDK layer SHA256 as R1353:
3727f0a7f966b9fae8b3cdbe70e976d2bd28a4c1409c525278ebd104577bc15a.
Set DEVKITPRO and DEVKITPPC, then run sh tools/build_r1354.sh.
No SDK, BIOS, games or user logs are redistributed.

Additional synthetic warm scheduler test(all new IOP options ON):880986 PPC
instructions vs909668 in the initial R1354 with those IOP options OFF, roughly
3.2% less in that test. Cold compilation costs can rise. This is not a measured
Wii BIOS speedup. The final477-program IOP and new combined26-state scheduler
tests passed; source, delay, timer and shared-register boundaries remain guarded.
