# PS2Recomp strict parser — Batch A12: pure integer ALU class

Date: 2026-11-08 · Branch `codex/ps2recomp-bridge-batch-a-memory` (PR #58, Draft)
Pinned upstream: `e5636981a1c9de368f6b71e819c3dc188dfedbf4`

## What changed

`tools/ps2recomp_strict_parser.py` now additionally accepts:

| Class | Instructions | Upstream template (verified in real output) |
|---|---|---|
| I-type, writes `rt` | SLTI, SLTIU, ANDI, ORI, XORI | `SET_GPR_U64(ctx, rt, …)` with `(uint16_t)` zero-extended or `(int32_t)` sign-extended-as-uint32 immediates |
| LUI (`rs == 0`) | LUI | `SET_GPR_S32(ctx, rt, (int32_t)((uint32_t)IMM << 16));` |
| R-type, writes `rd`, shamt 0 | SUBU, AND, OR, NOR, SLT, SLTU (ADDU/XOR from A11) | `SET_GPR_S32`/`SET_GPR_U64` forms in `R_TYPE` |

The decoder is now table driven (`I_TYPE`, `R_TYPE`); anything not in a table
or special-cased is still rejected. Destination `$zero` is accepted for the new
instructions only because the real generator emits ordinary setter statements
for it (macros skip register 0) — see the `a12_zero_dest` function. ADDIU→`$zero`
and SLL→`$zero` stay rejected (distinct upstream output).

## Evidence

- New real-generator fixture `tests/fixtures/ps2recomp_a12/` (3 functions, 29
  instructions, upstream report: 0 unhandled, 0 warnings/errors), unmodified
  output of the pinned `ps2_recomp`; ELF reproducible via
  `tools/ps2recomp_fixture_elf.py <out> a12`; hashes in `PROVENANCE.json`.
- Parser tests: 29 OK, including an exhaustive "bump every number in every
  statement of both real golden files" mutation test (all rejected), operator
  and type-confusion mutations (`|`→`&`, signed↔unsigned compare, ADD32↔SUB32,
  shift 16→15), reserved-field cases and unsupported neighbours (ADD/SUB/
  MOVZ/MOVN/MULT/DIV/REGIMM).
- Differential (`tests/test_ps2recomp_a11_aot_diff.py`, now looping over both
  fixtures): A12 = 360 parsed-function/interpreter comparisons (288 partial
  budget) + 9 live-source guard cases; A11 unchanged at 240 + 6. Inputs include
  all-zero, all-ones, `0x8000000080000000` and `0x7fffffff` register patterns
  (sign/unsigned compare edges) plus random 128-bit values.

Host-native only; no Wii/Dolphin result. Limits are those of A11 (straight
line, ≤32 instructions, entry at function start, no memory/branches).
