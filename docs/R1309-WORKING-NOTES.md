# Gekko2 R1309 development — VU control and IOP timer events

This is a verified source milestone after R1308, not a completed VU/event
scheduling phase or a packaged hardware release. Internal cross-builds retain
the R1308 application banner. Points 4 and 5 of the dynarec plan remain open.

## VU control corrections

- A taken branch in an older branch's delay slot preserves both redirects.
  The younger delay slot executes at the older target before the younger
  redirect takes effect. Untaken younger branches do not arm a redirect.
- BAL/JALR in a branch delay slot link to `(older_target + 8) / 8` rather
  than the sequential pair. Both the scalar implementation and native PPC
  emitter preserve the target before an aliased source/link write. VI0
  continues to discard link writes.
- D/T use VU0's shared FBRST, including VU1 traps. Enabled D/T set the
  appropriate VPU_STAT bits and raise INTC VU0 or VU1. Both halves of the
  marked pair retire, then execution stops without the extra E delay pair.
  Disabled traps leave E-bit behavior intact. Native block admission already
  excludes marked control pairs, so these events stay in the pair scheduler.

Primary references inspected: PCSX2 `pcsx2/VUops.cpp`,
`pcsx2/VU0microInterp.cpp` and `pcsx2/VU1microInterp.cpp`. Existing PCSX2
credits and GPL-3.0 licensing apply.

## IOP timer event-free intervals

The normal IOP scheduler still ticks peripherals once per IOP step. The
timer tick now defers count updates only before a proven target/wrap
boundary. The original scalar transition executes the exact boundary tick,
with unchanged one-shot/repeat, zero-return and target/overflow flag behavior.
MMIO reads/writes materialize all outstanding counts. Writes invalidate the
derived distance. Checkpoint ITMR serialization uses snapshot/restore without
changing its stored layout; restore invalidates derived state. Retained mutable
state pointers conservatively select scalar ticking until reset.

TARGET=0 with zero-return is recognized as a persistent dense-event case,
avoiding a repeated distance scan. This optimization preserves the existing
timer approximation; it does not add prescalers, gate timing, toggle polarity
or repair the old 32-bit overflow model.

## Validation

- 214/214 host tests pass, including 30,000 randomized IOP timer MMIO/tick
  transactions with IRQ counts checked after every tick, wrap/zero target,
  snapshot/restore and held mutable pointers.
- JIT and Interpreter linked PPC builds pass 6,912 IOP timer ticks each
  against an independent Python state/IRQ oracle, including mixed MMIO.
- Both linked PPC builds pass 32 existing VU Q/P/EFU/hazard oracles, 48 D/T
  gate/shared-control/E-priority/full-pair cases and three nested branch cases.
  The JIT also passes seven native block groups and 18 independent native
  BAL/JALR normal/nested/alias/VI0 link cases.
- 11,232 native fused-pair and 9,600 native block differential/ABI cases pass.
- 477 paired whole-state IOP programs and exact EE8/IOP1 ordering pass with
  signature `c10b06f395e375e71c13261205cc89a732b9970ee3911ab78870a8225bb1be4f`.

## Measured PPC instructions in 4,096 isolated IOP timer ticks

All six timers use MODE=0x58. Count and final I_STAT agree with R1308.

| TARGET | R1308 | R1309 | Change |
| --- | ---: | ---: | ---: |
| 100000 | 581632 | 45457 | 92.2% fewer instructions |
| 64 | 595456 | 84992 | 85.7% fewer instructions |
| 0 | 1368064 | 1417255 | 3.6% more instructions |

The dense-event regression is explicit. These are isolated timer costs,
not whole-emulator speed, BIOS responsiveness, or measured physical-Wii FPS.

## Remaining work for points 4 and 5

Full FMAC flag production/visibility, lane scoreboards, IALU/VI branch hazards,
M-bit synchronization, asynchronous VU/VIF/GIF/XGKICK coordination, and general
Count/Compare/SIF/GS event batching remain unimplemented. Native pairs/blocks
must participate in those contracts before the phases can be marked complete.
The code does not certify stable current-build OSDSYS frames on a physical Wii.
