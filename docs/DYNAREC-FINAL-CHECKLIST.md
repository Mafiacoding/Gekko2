# Gekko2 final dynarec checklist

This checklist tracks the remaining work from the verified R1323 baseline. A phase is complete only when its implementation, focused tests, full host regression suite, and relevant cross-build/hardware gates pass. Unsupported architectural edge cases may retain a deliberate scalar fallback, but only when the fallback is explicit, tested, and cannot silently execute stale or incorrect native code.

## 1. VU0/VU1 pipeline and side effects

- [x] Ordinary VU lower forms have no native coverage gaps.
- [x] VU upper interpreter-backed canonical forms audited native (R1324: 95/95).
- [ ] DIV, SQRT, RSQRT, WAITQ timing/visibility.
- [ ] EFU/P pipeline issue, completion and WAITP semantics.
- [ ] XTOP, XITOP and XGKICK side effects and VIF/GIF interaction.
- [ ] Upper/lower pair hazards, Q/P dependencies, E-bit/end handling and branch boundaries.
- [ ] Independent microprogram state/timing tests plus full host regression.

## 2. Register allocation/residency completion

- [x] Scalar word residency and generation tracking across audited callbacks.
- [x] Profitable paired 64-bit residency.
- [ ] 128-bit/vector residency and liveness/dirty masks.
- [ ] Wider helper contracts and spill/flush/reload rules.
- [ ] Safe inter-block residency contract.
- [ ] PPC EABI/exception/context-switch oracles and full regression.

## 3. Native block linking and cache finalization

- [x] Source/mapping generations, CPU/DMA invalidation and collision-safe eviction.
- [x] Warm successor chaining with live guards.
- [ ] Direct legal tail links / patched successors.
- [ ] Unlink/relink on source, TLB, ASID, DMA and reset/checkpoint changes.
- [ ] Allocation-failure, collision, eviction and active-code pinning parity under direct links.
- [ ] Cross-built PPC tests and full regression.

## 4. EE control/exception closure

- [x] Structural SPECIAL/REGIMM/COP0/COP1/COP2/MMI reachability audits.
- [x] Main link/likely/BC1 families audited.
- [ ] Finish delay-slot fusion boundaries and nested control cases.
- [ ] EPC/BD and IRQ-at-every-boundary tests.
- [ ] Mutated delay-slot source and exceptional COP0/system cases.
- [ ] Explicit tested scalar fallback for intentionally non-native privileged cases.

## 5. IOP completion audit

- [x] Precise cached native blocks integrated with EE8/IOP1 scheduler.
- [x] Load delay, merge forwarding, EPC/BD/TAR, alignment and overflow precision.
- [ ] Remaining HLE/context-switch and hardware-accuracy audit.
- [ ] Safe fast boundaries without changing scheduler/IRQ ordering.
- [ ] Repeated BIOS/game-path parity tests and full regression.

## 6. Event batching and Wii acceptance

- [ ] Identify/prove event-free execution intervals.
- [ ] Batch only intervals that preserve Count/Compare, timers, DMA, SIF, VBLANK and IRQ boundaries.
- [ ] devkitPPC cross-build after all prior phases are green.
- [ ] Repeated physical-Wii coldboot and OSDSYS navigation.
- [ ] Game smoke tests and profiling on identical hardware/settings.
- [ ] Final ELF/DOL checkpoint only after the above gates pass.

## Promotion rule

Work proceeds in order 1 -> 6. Each phase gets a backup branch before invasive changes, focused audits/tests first, then `bash tests/run_test.sh --all`. The verified development branch is advanced only by fast-forward after the phase is green. No physical-Wii success is claimed from host tests alone.
