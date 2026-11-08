# Strict PS2Recomp generated-C++ parser and AOT lowering — Batch A11

Date: 2026-11-08
Branch: `codex/ps2recomp-bridge-batch-a-memory` (PR #58, stays Draft)
Pinned upstream: `ran-j/PS2Recomp` `e5636981a1c9de368f6b71e819c3dc188dfedbf4`

## Result

Implemented and tested (host-native only):

- `tools/ps2recomp_strict_parser.py` — fail-closed parser for one straight-line
  integer function of real PS2Recomp-generated C++.
- Lowering of the validated raw words through the **existing**
  `tools/ee_aot_generate.py --manifest` CLI into Gekko2's AOT entry table.
- A real golden fixture produced by building and running the pinned upstream
  generator (not hand-written).
- 24 parser unit tests and a 240-case parser→AOT→live-core differential.

Not implemented: no upstream C++ is compiled or executed, no `R5900Context`
is used, no new scheduler path, no Wii/Dolphin/hardware result.

## Golden fixture origin (real upstream output)

`tests/fixtures/ps2recomp_a11/ps2_recompiled_functions.cpp` is the unmodified
single-file output of `ps2xRecomp/ps2_recomp` built from the pinned commit
(g++ 13.3.0, cmake 3.28.3, ninja; dependencies fetched by upstream's own
CMake at its pinned tags). Input was a synthetic ELF built by
`tools/ps2recomp_fixture_elf.py` from hand-assembled public words (no game,
BIOS or disc data). Config: `a11.toml` (syscall/COP0/CACHE patching off).
Upstream report: 2 functions, 0 unhandled instructions, 0 warnings/errors.
Hashes and build facts: `tests/fixtures/ps2recomp_a11/PROVENANCE.json`.
The ELF is not committed; a
test checks that the script still reproduces the recorded ELF hash.

Two functions are in the file: `a11_int_leaf_0x100000` (ADDIU, ADDIU with
negative immediate, ADDU, XOR, SLL, NOP) and `a11_zero_dest_0x100018`
(ADDU/XOR with `$zero` destination, ADDIU `-0x8000`/`0x7fff`, SLL 31,
aliased registers).

### Corrections to the A10 source-only audit (found by running the real binary)

- Function name is `<name>_0x<start>` (e.g. `a11_int_leaf_0x100000`).
- Negative ADDIU immediates are emitted as **unsigned 32-bit decimal**
  (`4294967295`), not signed.
- An extra `ctx->pc = START;` precedes the first instruction comment.
- Raw words/addresses are minimal lowercase hex (`0x1095021`, `0x0`).
- A disassembly text follows the raw word after two blanks.
- `#ifdef _DEBUG` / include preamble exists at file level (ignored, outside
  the function); the in-function `PS2_FUNCTION_LOG_TRACKER` block is always
  emitted by upstream.

## Parser architecture

1. `normalize()`: reject non-ASCII, NUL, `/* */`, line continuations;
   collapse whitespace runs, drop blank lines. Tokens are never rewritten.
2. Select the function: exactly one `void NAME(` line; name must be a C
   identifier; the line must equal the exact pinned signature
   `void NAME(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {`.
3. The two preceding lines must be `// Function: …` and
   `// Address: 0xSTART - 0xEND`; START nonzero/aligned, END aligned > START,
   at most 32 instructions (AOT block limit); a `_0x…` name suffix must equal
   START.
4. Body grammar, every line consumed exactly once: optional exact logging
   preamble (3 lines, name must match) → `ctx->pc = START;` → per instruction
   `// ADDR: WORD[  disasm]`, `ctx->pc = ADDR;`, statement → `ctx->pc = END;`
   → `}`. Numbers must be canonical lowercase hex. Addresses must be
   contiguous from START; the disassembly mnemonic (if present) must agree.
5. `expected_statement(word)` decodes the raw word and rebuilds the exact
   upstream statement; the generated line must equal it.
6. Any deviation raises `ParseError`; the CLI exits 2 and writes no output.

## Supported / rejected

| Accepted | Exact reconstructed statement |
|---|---|
| ADDIU (`rt != 0`) | `SET_GPR_S32(ctx, rt, (int32_t)ADD32(GPR_U32(ctx, rs), <simm as uint32>));` |
| ADDU (shamt 0) | `SET_GPR_S32(ctx, rd, (int32_t)ADD32(GPR_U32(ctx, rs), GPR_U32(ctx, rt)));` |
| XOR (shamt 0) | `SET_GPR_U64(ctx, rd, GPR_U64(ctx, rs) ^ GPR_U64(ctx, rt));` |
| SLL (`rs == 0`, `rd != 0`) | `SET_GPR_S32(ctx, rd, (int32_t)SLL32(GPR_U32(ctx, rt), sa));` |
| NOP (word exactly 0) | `// NOP` |

ADDU/XOR with `rd == $zero` are accepted (upstream's setter macros skip reg 0;
Gekko2's AOT emits a no-op) — covered by the real `a11_zero_dest` function.

Rejected (each has a test): wrong/missing signature, duplicate or missing
function, missing/misordered comments, PC gaps/mismatches/non-canonical
numbers, wrong end address or fallthrough, changed/extra statements (also
trailing comments), labels, `goto`, `switch`, branches/jumps, loads/stores and
memory macros, helper/runtime calls, syscalls, COP0/FPU/VU/MMI and every other
opcode, ADDIU→`$zero`, SLL→`$zero`, non-canonical NOP aliases, nonzero
reserved fields, any preprocessor line except the exact preamble, block
comments, non-ASCII, >32 instructions.

## Decoder and ABI boundaries

- The Python decoder is deliberately five cases wide; it is not a general
  MIPS decoder. The raw word is the authority and the C++ is a cross-check.
- Operand text of the disassembly is not interpreted (only the mnemonic).
- Upstream macros (`SET_GPR_S32`, `ADD32`, …) are *not* evaluated. The match
  relies on the audited upstream sources (A10) plus the real-output fixture.
  Upstream `Ps2SetGprLow64` replaces only the low 64-bit lane (checked in the
  pinned source), consistent with the AOT's `ud0` write; this is a fact about
  these five statements, not a proof of the two runtimes' full register model.
- `PS_LOG_ENTRY` (logging only) is ignored because the generated C++ is never
  run. `rdram`, `runtime` and `R5900Context*` are never used; there is no cast
  to `ee_state_t*`.

## AOT lowering

`lower()` writes a temporary manifest `{blocks:[{pc, words}]}` containing only
the validated words and calls `tools/ee_aot_generate.py --manifest`. The result
is a normal `ee_aot_generated.inc` entry keyed by the verified start PC and
first word. At run time it uses the existing contract unchanged:
`ee_core_step_n()` → `ee_step_budget()` → `ee_aot_try_execute_fetched()`
(budget clamped by `ee_core_interleave_limit()`), window begin
`ee_core_native_window_begin_aot()` followed by exactly one
`ee_core_interleave_account(fast)`, or per-instruction
`ee_core_interleave_account(1)` in the scalar tail. Live-source word checks
remain. Nothing in A11 touches the scheduler or bridge native-window API.
A JSON report records parser version, upstream commit, source and AOT SHA-256.

CLI example (writes outside the checkout in real use):

```sh
python3 tools/ps2recomp_strict_parser.py --source ps2_recompiled_functions.cpp \
  --function a11_int_leaf_0x100000 --output /tmp/ee_aot_generated.inc
```

## Tests and results (host-native, this container)

| Command | Result |
|---|---|
| `python3 tests/test_ps2recomp_strict_parser.py` | 24 tests OK |
| `python3 tests/test_ps2recomp_a11_aot_diff.py` | PASS: 240 parsed-function/interpreter comparisons (192 partial-budget), 2 functions, 6 live-source guard cases |
| `python3 tests/test_ee_aot_inputs.py` | 4 OK |
| `python3 tests/test_native_hybrid_compile.py` | 5 OK |
| `python3 experiments/ps2recomp-wii/run_hybrid_test.py` | PASS 512/512 |
| `tests/run_test.sh` bridge, bridge_window, residency_generation, exec_lifecycle | PASS |

The differential compares, interpreter vs AOT: all 32 128-bit GPRs, HI/LO,
SA, PC/next-PC, branch flag, instruction count, COP0 (Count), exception and
TLB state, FPR/VU0 state and exact retired counts across budget splits
(full, 1, 2, 3, n−1). It additionally asserts `$zero` stays 0, every upper
64-bit lane is unchanged, only the function's destination registers' low lanes
change, and that the AOT counter advanced (so the native path really ran). A
sensitivity check (manually altering a generated shift) made the differential
abort, i.e. it detects wrong AOT semantics. Live-source mutation (first, last
and middle word rewritten) never runs stale native code.

The mutation guard means a changed word makes native execution stop and the
interpreter finish — so the differential proves safety, not that every
mutated variant is accelerated.

## Known limitations

- Subset: 5 opcodes, straight-line, ≤32 instructions, no memory/branches.
- A single function per AOT block; no call graph, no dispatch from other
  recompiled functions (`func(rdram, ctx, runtime)` calls are rejected).
- Entry only at the exact function start PC; a mid-function resume uses the
  interpreter.
- The fixture covers only the audited templates; wider generator options
  (comments disabled, other emitter modes, `single_file_output=false`) are
  untested and either rejected or unvalidated.
- Real upstream was run on Linux x86-64 only; the host differential uses an
  x86-64 build of Gekko2's core.
- No Wii cross-build: this container has no devkitPPC (`DEVKITPRO` unset,
  no `/opt/devkitpro`, no `powerpc-eabi-gcc`), so `tools/build_ee_aot.sh`
  could not run. The generated include is plain C already exercised by the
  host core; PPC compilation is unverified. No Dolphin/Wii/FPS claims.

## Next step (A12)

Add exactly one new instruction class at a time, each with a real-generator
fixture and decoder test, then differential. The natural next step is
`SLTI/SLTIU/ANDI/ORI/XORI/LUI` plus `OR/AND/NOR/SLT/SLTU/SUBU` (pure integer,
same macro family), then a block-terminal `jr $ra` with delay slot so real
leaf functions are admissible. Also a manifest tool that selects functions
from a generated file by address list and writes the private lowering report
outside the checkout, and a PPC/devkitPPC cross-build of an AOT include
generated by this path.
