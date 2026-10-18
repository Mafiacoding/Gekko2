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
