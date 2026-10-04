# R1294 — precise EE blocks and the CT24 GX blocker

2026-10-04. Early alpha. Full EE/IOP/VU recompilation and full GS hardware rendering remain incomplete. R1294 has not yet been tested on a physical Wii.

## Supplied R1293 hardware log

`R1293-gx-render.log` reaches FIRST_IMAGE at 64,639 ms / EE63,984,592, EE PC003578b8, with a disc present. DISPFB1/2=948c selects PSMCT24; the displayed region is 640x224. Across 34 performance samples, GX readiness, output attempts, primitive attempts and accepted draws remain zero. The main output gate admitted PSMCT32 only, so it never called the presentation backend and never initialized GX. Zero work here does not measure GPU drawing performance.

Median core time is 98.43% of the interval and median sampled EE share is 81%. Core includes GS helpers invoked during execution. Different media/guest PCs and first-image timing make this unsuitable as a controlled comparison with older diskless BIOS runs. No game identity, stable OSDSYS navigation or physical R1294 FPS is established by this log.

## EE block implementation

A separate precise native executor now runs bounded 2–8 instruction ALU blocks from the existing eight-EE/one-IOP scheduler. It accepts ADDIU, ANDI/ORI/XORI/LUI, fixed/variable 32-bit shifts, ADDU/SUBU, 64-bit logical operations, SLT/SLTU and DADDU/DSUBU. It does not accept loads/stores, branches, trapping arithmetic, COP instructions, HI/LO or VU execution.

The old per-instruction retirement epilogue is shared: zero register, instruction count, Count/Compare, IRQ timing, VBlank, timers, SIF/RPC and interrupt delivery still run after every instruction. Native code invokes a prepare callback before each instruction. It checks the current PC/NPC, halted/idle/delay state, actual mapping and source encoding, and stops before stale code or changed control flow executes. Results are stored in architectural state before callbacks. PPC callee-saved registers and LR/SP are preserved.

Formation translates once within the same 4 KiB source page and respects backing-store bounds. It never speculatively reads devices or raises faults. RAM below physical 0x200000 stays on the historical guarded step path. The real scalar fetch supplies its current backing pointer for eligibility; this is not a persistent mapping cache. Normal fetch faults and delay slots retain the scalar path.

The cache is bounded to 256 PC-tagged entries. Warm code skips the duplicate whole-block scan because the emitted prepare callbacks validate every instruction live. If the first word/mapping changes, native code returns before its buffer is released; scalar execution handles the current instruction and later visits may compile again. Replaced buffers are freed only after a new buffer finalizes successfully. Allocation failures decline without advancing the guest and can retry. Budget limits are never overshot. No cross-block linking or register residency across callbacks is implemented yet.

`EE_BLOCK runs/retired` is separate from scalar fragment counters. HUD native-instruction coverage includes both. `GS_STATE` is a periodic state snapshot, not an aggregate classification of every rejected draw.

## GX changes

The presentation policy accepts CT32 and CT24 after the first image and retains unsupported-format fallback. Both formats share RGB swizzled addressing; presentation already ignores the high alpha byte. This allows backend initialization in the supplied display configuration without forcing guest GS registers.

Eligible opaque untextured flat triangles/sprites can now target CT24 as well as CT32. CT24 masked readback replaces RGB only and preserves the destination alpha byte. SCISSOR, SCANMSK, winding, inclusive triangle coverage, exclusive sprite bounds and padded/uncovered VRAM remain protected. CT32 software row-fill behavior is unchanged. Unsupported alpha/blend/fog/dither/mask/depth/textured/Gouraud-triangle states retain software fallback.

GX drawing still prepares coverage on the CPU and uses exact row-span quads with deferred readback; this is not a complete hardware GS renderer. GPU synchronization/readback per accepted primitive remains a likely cost. Native CPU operations belong on PPC; GX handles graphics and is not a general EE/IOP/VU instruction executor.

## Validation

- Both devkitPPC/libogc ELF and DOL pairs build successfully.
- 192/192 native tests pass, including CT24/32 output gates and independent full-VRAM CT24/32 import oracles with preserved alpha, clipping, winding, masks, bounds and padding.
- 12 linked PPC jobs pass across both engines: precise blocks, GX submission/routing/ownership, boot policy, GS paths, profiler/Remote/FPS/scheduler, and IOP control/VU integration. Platform allocation/cache/GX calls are mocked; guest/generated PPC instructions actually execute.
- Block tests cover budgets 2/3/5/8, full register-state parity for all supported ALU encodings, Count/PC, source replacement including warm entries, mid-block TLB replacement, actual Count/Compare interrupts at every position 1–8 with correct EPC/vector, delay/idle/halt rejection, source-page boundaries, cache collisions, allocation retry and PPC callee-saved registers.
- The independent ALU register-state digest matches R1293 and the current interpreter. All 66 full 4 MiB VRAM signatures per engine match R1293.
- Eight eligible mapped ADDIU, complete warm retirement: R1293 JIT4321 -> R1294 JIT3535 PPC instructions, about 18.19% fewer; current interpreter4297. This is a selected synthetic workload, not a Wii cycle/FPS prediction.
- Guarded low-physical kernel fixtures still use C: mapped ADDIU4321 ->4567 and LW5209 ->5481 (~5.69% /5.22% extra overhead). Earlier unrestricted probing was worse; using the already-fetched word/backing reduces it. Remaining selection/fallback overhead must be measured and reduced. R1294 is not a universal speedup.

Authoritative reports and final logs are in the checkpoint's verification directory. Intermediate failed fixtures/build attempts are not packaged as passing evidence. No BIOS, game images or raw uploaded logs are included.

## Install and next hardware comparison

Copy `PCSX2-Wii-R1294-Menu-JIT.dol` as `sd:/apps/pcsx2-wii/boot.dol`. Keep the existing private BIOS and SD data. RIGHT toggles GX output; LEFT enables experimental flat rendering. Both wait for a genuine first image. Logs are `sd:/pcsx2/R1294-software.log`, `R1294-gx-output.log`, and `R1294-gx-render.log`; a fresh launch may overwrite that same mode's previous log, so copy it before repeating the mode.

Repeat identical BIOS/media/settings for software, GX output and GX drawing. Check readiness and real GX_WORK submissions, synchronization failures, block retirement, throughput and guest VBlank. A changed source display format can still require supported-format fallback. The included interpreter build provides a comparison path.

Next: reduce block-selection/scalar overhead; measured memory/control-flow blocks with precise faults/delay slots; event scheduling optimizations with boundary proof; complete IOP/VU coverage; renderer target/texture residency and safe batching; textured/alpha/depth GX coverage. Preserve BIOS/OSDSYS stability ahead of game claims. The cumulative Claude patch/diff applies to the originally supplied full-debug project archive, not on top of an already patched R1293 tree.
