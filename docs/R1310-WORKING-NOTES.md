# R1310 unreleased EE instruction work

The complete dynarec plan remains open. No R1310 release binaries or checkpoint
are delivered. Physical Wii BIOS/OSDSYS stability and FPS are unverified.

## Changes

- Add direct PPC bodies for 43 previously declined MMI forms: comparisons,
  extrema, signed saturation, unsigned subtraction saturation, absolute value,
  PADSBH, color packing, halfword/word permutations, variable word shifts and
  paired multiply/divide/accumulator operations. All use caller-saved registers
  and remain within the existing 128-word per-instruction reservation.
- Independent packed lanes use a bounded loop rather than oversized unrolled
  byte saturation code. Permutations capture all source lanes before writing
  an aliased destination. HI/LO side effects remain when Rd is zero.
- Correct PMADDW/PMSUBW Rd to concatenate the low words of HI and LO, as in
  the vendored PCSX2 MMI.cpp. The previous scalar engine sign-extended LO.
  Preserve the lower-pipe PMADDW correction and truncating division by
  2^32-1. Perform wrapping accumulator arithmetic in unsigned C types.
- Add the six missing REGIMM immediate traps and native predicates for all
  twelve integer trap forms. Preserve immediate sign extension even for
  unsigned comparisons, 64-bit operands, EPC/BD and nested-EXL behavior.
  Set the synchronous exception marker so retirement cannot substitute a
  timer interrupt. Trap bodies also join precise blocks and fused delay slots.

## Verification

- 51,600 independent raw-PPC integer-oracle cases for the 43 new MMI forms,
  checking the full context, aliases, zero, extremes, PPC EABI preservation
  and code capacity. Largest emitted body observed: 76 words.
- 7,740 independent full GPR/HI/LO cases in each actual linked PPC JIT and
  interpreter build.
- All 103 legal MMI encoding/mode cases from the vendored primary opcode
  tables are accepted by the actual backend. Acceptance alone does not prove
  correctness of all older emitters or absence of C helper calls.
- 6,144 architectural trap cases in each linked PPC JIT/interpreter build;
  native execution counters are required in the JIT oracle. Another 6,144
  host cases check trap delivery, plus synchronous-versus-timer priority.
- The existing 214 host tests pass; the new host trap test passes separately.

Platform allocation/cache services are mocked in linked PPC tests. These are
correctness tests, not real Wii performance measurements. Larger COP/VU audits,
general liveness allocation, direct generation-guarded links, event batching,
IOP/HLE audits and physical Wii validation remain necessary.
