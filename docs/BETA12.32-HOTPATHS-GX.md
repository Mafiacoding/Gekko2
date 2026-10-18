# BETA12.32 general EE hotpaths and GX investigation

Baseline BETA12.31e / `90ee881`, branch `codex/beta12-32-hotpaths-gx`.
Two generic CPU changes and one GX CPU-overhead reduction, no game-ID/BIOS-PC specialization:

1. Match the frontend's native-code admission floor to the backend's existing
   1 MiB physical RAM policy. Previously 1–2 MiB code repeatedly attempted other
   execution routes while the precise block backend remained inaccessible.
   The low kernel exclusion and real fetch/source, TLB/ASID, DMA/SMC, exception,
   delay-slot, IRQ, scheduler-budget and PPC ABI protections remain.
2. Compute the precise-cache best-way budget interval in its first four-way
   traversal. The largest fitting owner cannot have another fitting larger
   owner; the interval's next threshold is among the already-seen >budget
   owners. Remove the redundant second metadata traversal. Cache mutation and
   invalidation rules are unchanged.

## Existing hardware data used privately

The owner-provided complete Wii Limits 1.1 CSV reports 280 records, no failed
checks and no GX timeout. It measures isolated kernels, not emulator FPS. In
particular the simple 256x256 textured batches/fence work, whereas the emulator
previously failed to observe the next PE token during a native renderer queue.
Do not label that as a lost interrupt: an idle CP and older callback do not
prove the requested fence completed. Do not extend timeouts, accept stale
completion or discard pending VRAM and pretend rendering succeeded.

The BETA12.31e UDP capture has real runtime samples, including dispatch and
scheduler overhead, but GX was OFF; it cannot validate GPU rendering or its
speed. Uploaded logs, addresses, BIOS assets and benchmark files remain private.

## GX scope and limits

Add an actual-ELF vertex-stream validator: modeled descriptor setup and actual
PPC FIFO payload writes must have exactly the advertised attribute bytes for
all 48 synthetic color/fog/TFX/source-alpha/additive-alpha combinations, including
four safely rejected HIGHLIGHT/source-alpha cases. Position-only counters did
not prove this. The 1,152 accepted primitives in the baseline have valid payload
sizes, so this test does not reproduce or fix the physical GX timeout. SDK
state calls are modeled and GPU rasterization is not executed. The original
GX fence failure remains OPEN; no hardware workaround is silently enabled.

## Verification

The new low-RAM differential uses six physical placements, including 4 KiB
and 2 MiB crossings, a retained low-kernel case, warmed code and scalar oracle.
Compare GPR128, PC/nextPC, COP0 and exact retirement. Count executed PPC
instructions independently from wall time: compile costs excluded, no claims
of Wii cycles/FPS from instruction counts. Existing budget-range mutation and
chain/memory/IRQ regressions remain required. Final results follow below.

## GX descriptor-copy optimization

The resident textured route copied the entire gs_gx_texture_draw for every
PABE/ordinary pass just to override hardware_blend. That includes 1,152
coordinate-map entries and 1,024 span endpoints. Pass the blend selector as
one argument instead. Keep the original immutable draw descriptor, shader
program, PABE order, emitted secondary coordinates and ownership fences.
This removes repeated stack copies; it is not a claimed fix for the observed
missing PE token. Compare stable GX configuration arguments and all actual PPC
vertex FIFO bytes against the old ELF, including resident shaded and additive
routes. No new experimental option/default or arbitrary hardware timeout.

## Resident submission fence coverage

The existing GEKKO2_GX_RESIDENT_FENCE switch already fenced the older resident
textured route. It did NOT fence the later native-triangle path, seed uploads
or resident flat draws. The previous owner timeout accumulated 320 begins
before its ownership wait; a native source-alpha draw was among recent work.
That correlation alone does not identify the exclusive hardware cause.

Apply the existing switch consistently to these submission boundaries after
marking GPU ownership, including all seed chunks. Use the same bounded fresh
PE-token proof and sticky failure path, not a longer timeout or CP idle as
success. Preserve unresolved VRAM ownership on failure. GX_SYNC now reports
submit/submit_tb separately from reuse, readback and presentation waits.

Fresh-frame guards serialize submissions and can reduce CPU/GP overlap; no
FPS improvement is claimed. Build GX_RESIDENT_FENCE=0 explicitly produces an
AsyncControl artifact for a controlled comparison without changing runtime
preferences. This is an implemented guard-coverage correction; whether it
eliminates the observed real-Wii timeout is still hardware-unverified.

Also cap masked native queues at 64 GX primitives between completed guards:
one guest triangle can expand into hundreds of scissored GX triangles. Mark
GPU ownership before the first native command so a failed intermediate fence
cannot expose stale canonical VRAM. The existing build switch controls this
coverage; AsyncControl deliberately omits these guards. A tall synthetic
triangle checks exact shader/payload equivalence, maximum queued begins and
failure specifically at submission reason 3, with pending ownership retained.

## Final results / delivery

Wii code build: `edf0674`; devkitPPC r50 GCC16.1.0, libogc3.1.0, O1 and prior
SPEED/DSI/AOT/GX flags. Factory experiment defaults and saved preferences are
unchanged. Primary SD app contains chain-capable code with the existing runtime
experiment gate. Review binaries also include the chain-free control and an
explicit AsyncControl. The latter omits submission guards for comparison;
ordinary memory-ownership fences remain. It is not the primary SD app.

- 83 host suites PASS. The existing network/config/menu/profile-work regression
  and linked network worker/IOS-boundary/transport checks PASS.
- 16 final-ELF PPC suites PASS, with each result recording its actual ELF SHA-256;
  the release packager rejects mismatched artifacts. These cover GPR128/HI/LO/SA,
  branches/delay exceptions, mapping/TLB/DMA-SMC, scheduler/IRQ and GS/GIF/VIF/IPU.
- Six source placements match the scalar GPR128, PC/nextPC, COP0 and retirement
  oracle, including page crossings, the newly admitted interval and excluded
  low-kernel text. Warm 64-ADDIU work at physical 1 MiB: **59,029 -> 35,494**
  executed PPC instructions (~39.9% less). This is a synthetic instruction-count
  comparison, NOT a Wii FPS or cycle measurement; compilation costs are excluded.
- Cache budget-range workload: 5,700 calls, **520,192 -> 512,292** PPC instructions
  (~1.5% less), same owner selection, mutation invalidation and interval hits.
- Actual resident texture stack frame: **9,192 -> 2,360 bytes**, verified in the
  delivered PPC disassembly. Avoid the 6,832-byte descriptor copy each pass.
- All **1,529** synthetic submitted primitives retain identical vertex payload
  and normalized shader/texture configuration fingerprints in old, guarded and
  async-control builds. Texture-object stack addresses are normalized; SDK
  calls, cache/IRQ/lazy-FPU services and IOS/GPU completion are modeled.
- Submission coverage: 44 accepted native draws get guards, four unsupported
  cases reject without submission. Tall triangle: **373 -> 64** maximum native
  begins before a completion fence (AsyncControl: 373). This is a CPU command
  submission bound, NOT a measured physical GP queue depth.
- A deliberately stale token in the intermediate 64-begin fence fails at reason
  3, aborts once, retains pending VRAM and blocks subsequent GPU draws. The separate
  fence suite also checks lost callback, token wrap and stale idle rejection.
- Three fresh Wii DOL/ELF section checks and static MEM1 gates PASS. Primary
  static end `0x815e58f4`, remaining **2,205,452 bytes** (minimum 1 MiB).

The 280-row owner benchmark was used privately; no owner files are packaged.
The implemented fixes are general code admission, cache traversal, descriptor
copy elimination and missing/insufficient native submission barriers. They are
not a completed emulator or a proven BIOS/game graphics correction. **The real
Wii timeout and net speed effect remain unverified after these changes.**
Additional fences may reduce CPU/GP overlap; use the explicit control for an
isolated comparison rather than claiming a measured speed gain. No longer
watchdog timeout, fake completion, fabricated framebuffer or automatic PSM/risk
activation was introduced. Existing Windows Logger1.1 and saved IP/port remain
compatible; the next normal run can use the already-working capture workflow.
