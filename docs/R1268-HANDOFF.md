# R1268: BIOS-directed EE/VU0 coverage and first direct IOP backend

## What changed

R1267's PC+opcode-tagged rejection cache and bounded code ownership remain
included. R1268 adds 60 EE/macro encodings: DADDI/DADDIU, ADD/SUB, six missing
64-bit shifts, MULT1/MULTU1/DIV1/DIVU1, 37 VU0 ACC-destination operations,
eight Q/I arithmetic operations and VWAITQ. All use generated PPC code.

64-bit variable shifts cover all masked counts 0..63; immediate shifts cover
32..63. GPR aliases, register zero and upper-half preservation are tested.
Pipe1 multiply/divide writes only HI1/LO1, preserves pipe0, and defines
zero-divisor and signed-overflow results. Signed ADD/DADDI follow the current
interpreter's documented no-overflow-trap simplification; full integer trap
accuracy is a separate remaining requirement.

VU ACC and Q/I operations preserve masked lanes and use separate float32
multiply/add instructions. New operand loading explicitly supplies VF00's
(0,0,0,1) constant instead of trusting mutable backing storage. VWAITQ matches
the current synchronous-Q interpreter model. This ports arithmetic behavior,
not the full hardware MAC/status pipeline, denormal/NaN/saturation model or
VU microprogram compilation. Those remain open.

A distinct direct R3000A code generator now supports 32 IOP operations:
integer shifts, immediate arithmetic/logical/compare, register arithmetic/
logical/compare, HI/LO transfers, multiply and COP0 register transfers.
Its context uses genuine 32-bit IOP registers, not the EE's 128-bit stride.
The IOP CPU calls it only for one instruction's register computation; the
original HLE prelude, fetch/PC advancement, scheduler ticks and IRQ epilogue
remain in place. Loads/stores, jumps/branches, divide and exceptions still
interpret. Generated buffers have bounded ownership, PC+word rejection tags,
transient allocation retry and a matching compile-time JIT-disable guard.

## BIOS profile and speed interpretation

A native PAD-only Browser cycle from the authentic R1266 main-menu checkpoint
retires 120,816,562 additional EE instructions with HALT=0 at each input stage.
Within it, 896,625 observed instructions belong to the broad MMI/COP2 families;
477,329 visits used encodings the pre-R1268 translator declined. These include
98,784 visits to one MULT1 site and 108,451 visits each to the matrix's three
VMULAx/y/z sites. After the additions, the actual new translator accepts ALL
477,329 formerly unsupported visits in that recorded stream, leaving zero
of those particular encodings declined. This does not prove all EE/VU opcodes
are implemented or that the BIOS executed under the full PPC JIT here.

The observer executes the native interpreter and classifies raw instruction
words through the real code generator. It cannot measure Wii/Dolphin FPS.
Source: tools/profile_osdsys_jit_r1268.py; recorded log and weighted coverage
JSON are included. The earlier isolated cache benchmark gives 1 allocation
instead of 1,000 for 1,000 repeated unsupported calls. Neither measurement
supports a numerical FPS promise. R1268 appends EE/IOP cache counters to
`sd:/pcsx2/R1268-boot.log` for an actual target comparison.

Recorded controller sequence: CROSS for 15M EE / release for 60M; CIRCLE for
15M / release for 30M. Browser No data is shown at EE 1,271,182,761 /
PC 0x00271bf8; return to main menu at EE 1,316,675,250 / PC 0x00272908.
This is a functional input response check at bounded instruction checkpoints,
not a sub-frame latency measurement or an FPS timing test.

## Verification

- 178/178 native regression executables pass after IOP integration.
- 9,000 generated PPC cases verify the ten integer additions.
- 4,800 generated PPC cases verify pipe1 multiply/divide.
- 2,368 generated PPC cases verify 37 ACC macro encodings.
- 800 additional generated PPC cases verify Q/I and VWAITQ.
- 16,000 generated PPC cases verify all 32 initial IOP operations, plus five
  explicit decline checks for memory/control/exception instructions.
- Total distinct new generated-code cases: 32,968. The Q/I verifier also reruns
  the ACC cases, which are not counted a second time.
- Actual emitted-ELF verification inherits all R1267 checks and adds real
  EE frontend coverage, IOP hot dispatch/rejection/word replacement, CPU
  retirement/scheduler ticks, memory fallback and an IRQ boundary with the
  instruction's result retired and correct EPC. JIT-only checks cover transient
  allocation retry and bounded full-cache behavior. Final emitted totals: 203 JIT / 196 Interpreter. See individual test logs.
  Platform allocation/cache-maintenance calls are controlled by Unicorn;
  emitted CPU/frontend/translator code actually runs on the emulated PPC.

## Build and artifacts

Use tools/build_r1268.sh with devkitPPC/libogc. The Interpreter build disables
both EE and IOP JIT and is the baseline. The JIT build enables both verified
frontends and is experimental. Keep your own BIOS and SD configuration file.
The source checkpoint, ELF/DOL, cumulative original-project patch/diff, logs,
public screenshots and SHA-256 manifest are included. BIOS images, disc data,
raw RAM checkpoints and toolchain archives are excluded.

## Complete JIT work still required

This is a larger verified checkpoint, not a completed EE/IOP/VU port.
EE remaining opcode coverage, exception fidelity, memory helpers, register
allocation and linked multi-instruction blocks remain. Event boundaries must
preserve the existing timer, INTC/DMAC, VBLANK, SIF/RPC and scheduler behavior.
IOP remaining memory/control/divide/exception paths and block formation remain.
VU0/VU1 microcode needs a native backend with pair ordering, I/E/branch delays,
local memory and XGKICK/GIF semantics preserved, plus a complete flag/pipeline
model. VU macro arithmetic is not VU micro JIT. Further GS accuracy and real
Wii/Dolphin performance measurements remain open. Tekken was not rerun here.
