# R1269: VU micro arithmetic, IOP control and integer memory

## Result and scope

This checkpoint includes every R1268 change, and adds native PPC computation
for 75 VU upper encodings and 33 VU lower encodings. Both VU0 and VU1 use the
actual VF/VI/ACC/local-memory arrays supplied by the caller. No EE shadow copy
or interpreter wrapper is used for arithmetic. Upper I/E flags and pair
retirement remain in vu_micro_step; branch and end delay slots are retained.
Unsupported operations continue to interpret.

Upper emission relocates the existing tested macro arithmetic operands to the
separate micro arrays. Only arithmetic encodings are eligible: macro-only VI
operations and lower Q producers are excluded. VI and ACC pointers are saved
only when the emitted body uses them. The warm cache has a specialized upper
ABI, and lookup occurs before entering the large interpreter functions.
Local memory uses PPC byte-reversed accesses with the caller's VU memory mask.
Lower computation covers VI16 arithmetic/aliases, MOVE/MR32/MTIR/MFIR,
current-model WAITQ/WAITP, quad/scalar loads and stores, increments/decrements,
and ten ordinary branch/jump forms. Each cache has bounded code ownership;
upper flags share an arithmetic entry; rejected encodings are tagged;
allocation failure retries rather than becoming a permanent rejection.

Two missing upper interpreter instructions are now decoded from the bundled
primary PCSX2 VUops.cpp tables: MULA at special index 42 and OPMSUB at opcode
0x2e. OPMSUB always updates xyz, preserves w and captures source components
before writes. Its old shared PPC macro emitter had an aliasing bug; deferring
all three stores fixes both macro and micro use. JALR similarly captures its
VI target before an aliased link-register write, following primary _vuJALR.

The IOP backend grows from 32 to 54 encodings. New code covers BLTZ/BGEZ,
BEQ/BNE/BLEZ/BGTZ, J/JR, DIV/DIVU and all twelve base integer memory forms
(LB/LBU/LH/LHU/LW/SB/SH/SW/LWL/LWR/SWL/SWR). Branch targets use the runtime
instruction PC, allowing one encoding to be reused at different addresses.
JAL/JALR and link branches retain their HLE/interpreter paths. Generated
memory code calls real IOP helpers with the state/address/value ABI, preserving
BIOS/MMIO and Status.IsC behavior. Loads into zero still access memory.
Unaligned forms retain the read/modify/write behavior and all four offsets.
The IOP interpreter's undefined signed division overflow and stale HI/LO on
zero divisor are corrected to primary R3000AOpcodeTables.cpp psxDIV/psxDIVU.
The generated divider handles the same cases without undefined PPC division.
HLE prelude, PC advance, retirement, timer/scheduler and IRQ epilogue remain
in the existing core.

## Verification

* Native regression suite: 180/180 pass, including new IOP divide and VU pair
  I/E/branch/alias/decode regressions.
* Generated PPC VU micro: 4,800 upper + 5,280 lower cases = 10,080. All masks,
  VF destination aliases/zero, VI16 aliases, local-memory wrap/endian,
  ordinary branches, full untouched-state and callee-saved registers checked.
* Generated PPC IOP: 10,000 new control/divide and 4,800 memory cases; the
  existing 16,000 scalar cases remain passing. Memory mocks clobber volatile
  registers and independently check every helper access, data byte and GPR.
* Exact built Wii ELF checks: 217 in JIT / 208 in Interpreter. VU tests cover
  hot-cache ownership, shared generated code on separate arrays, I/E pair
  ordering, delayed retirement and retries/full cache. IOP tests cover all
  native memory gates, real branch/delay retirement and runtime-PC reuse,
  divide corners, actual MMIO read/ack, Status.IsC, every unaligned form/offset
  and loads to zero. Both builds retain original IRQ delivery checks.
* Authentic BIOS navigation is checked with only real PAD input. All four input stages finish with HALT=0; Browser "No data" and
  return to the main menu are captured. 120,874,910 additional EE instructions
  retire from EE=1,316,675,250 through EE=1,437,550,160; log/frames included.
  The native BIOS driver uses the interpreter; it is not a full PPC BIOS run.

## Performance evidence

The reproducible synthetic 302-pair warm VU1 loop executes identical state in
R1268 and R1269. Three repeated runs each count 65,248 versus 63,799 actual
PPC instructions: 2.2208% fewer. The first draft was 18.3% worse; moving the
JIT dispatch ahead of the interpreter prologue and specializing cache calls
removed that overhead. The final figure measures instruction count in one
small test, not hardware cycles, BIOS input latency or Wii FPS. Native memory
uses helper calls, not a proven direct-RAM fast path. No 5/10 FPS claim.

## Still open: full JIT is not complete

* EE: remaining MMI/COP/control coverage; accurate traps and floating point;
  real multi-instruction blocks with correct scheduler/event boundaries,
  register allocation and block linking.
* IOP: HLE-sensitive calls/link variants, RFE/exception paths, accurate load
  delays/overflow handling and linked blocks/direct RAM optimization.
* VU: MIN/MAX and conversion rows currently fall back, Q/EFU producers,
  flags, random/other missing lower operations, GIF/VIF helpers and multi-pair
  blocks. Full PS2 float saturation/denormals/NaNs/MAC/status, Q/P readiness,
  upper/lower parallel hazards and asynchronous scheduling remain open.
* VU local addressing retains the existing interpreter's simplified mask
  model and quad-load/store lane conventions; this port does not claim to
  repair every architectural memory alias or nested-branch behavior.
* Physical Wii/Dolphin throughput and response must still be measured.
  Tekken has not been retested in this revision.

## Reproduce

1. Run tools/verify_regressions.py for native objects/tests.
2. With Unicorn available, run tools/verify_jit_vu_micro_r1269.py and
   tools/verify_jit_iop_memory_r1269.py. The latter includes the prior scalar
   and new control/divide oracles; count each retained case once.
3. Set DEVKITPRO/DEVKITPPC plus compiler runtime dependencies and run
   tools/build_r1269.sh (-j1).
4. Run tools/verify_ppc_iop_control_r1269.py ELF --nm powerpc-eabi-nm for
   each build; it includes all previous shared/EE/IOP and new VU checks.
5. tools/compare_ppc_vu_r1269.py BEFORE_ELF AFTER_ELF --nm powerpc-eabi-nm
   repeats the synthetic warm instruction-count comparison.
6. Use a user-supplied matching BIOS and private authentic checkpoint with
   tools/verify_osdsys_navigation_r1266.c for controller-only menu tests.

The package excludes BIOS, game/disc images, extracted IRX payloads and raw
RAM checkpoints. Cumulative patch/diff are against the user's original
pcsx2-wii-full-debug archive, validated by applying and comparing exact bytes.
JIT and Interpreter ELF/DOL are both included as a fallback pair.
