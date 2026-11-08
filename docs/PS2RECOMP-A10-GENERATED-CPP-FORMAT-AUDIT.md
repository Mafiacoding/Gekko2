# Upstream PS2Recomp generated-C++ format audit — Batch A10

Date: 2026-11-08
Repository: `ran-j/PS2Recomp`
Pinned upstream commit: `e5636981a1c9de368f6b71e819c3dc188dfedbf4`
Gekko2 branch: `codex/ps2recomp-bridge-batch-a-memory`
PR: #58

## Result

The upstream generated-C++ format can now be audited directly from the exact
pinned generator source. A user upload is not required to establish the
function envelope and the common integer statement templates.

This is a **source-format audit**, not yet a parser, lowering implementation,
or runtime integration. The current Gekko2 PR tree contains 49 `.cpp` files;
all 49 are under `docs/reference/pcsx2/pcsx2/`. None is an upstream
PS2Recomp-generated game function. The existing unchanged generated-leaf
probe and its generated source are described as workspace-private in
`experiments/ps2recomp-wii/README.md`; they are not committed fixtures.

## Exact upstream source evidence

The pinned upstream `ps2xRecomp/src/lib/function_emitter.cpp` emits functions
with this signature:

```cpp
void <sanitized_name>(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {
```

For each ordinary instruction, when instruction comments are enabled (the
`CodeGenerator` default is enabled), the emitter writes:

1. a comment containing guest address and raw instruction word:
   `// 0xADDR: 0xWORD [disassembly]`;
2. `ctx->pc = 0xADDRu;`;
3. the C++ statement returned by the instruction translator.

For a straight-line function without control flow, the emitter finally writes
`ctx->pc = 0xENDu;`. The function emitter may also include the optional
`PS2_FUNCTION_LOG_TRACKER` preamble. The function's address range is emitted
in a preceding comment: `// Address: 0xSTART - 0xEND`.

Verified source files at the pinned commit:

- `ps2xRecomp/src/lib/function_emitter.cpp`
- `ps2xRecomp/src/lib/instruction_translator.cpp`
- `ps2xRecomp/src/lib/special_translator.cpp`
- `ps2xRecomp/src/lib/control_flow_emitter.cpp`
- `ps2xRecomp/include/ps2recomp/code_generator.h`
- `ps2xRuntime/include/ps2_runtime_macros.h`

## Confirmed scalar integer templates

The following exact statement templates are emitted by the pinned translators:

| R5900 instruction | Generated statement |
|---|---|
| ADDIU | `SET_GPR_S32(ctx, rt, (int32_t)ADD32(GPR_U32(ctx, rs), signed_imm));` |
| ADDU | `SET_GPR_S32(ctx, rd, (int32_t)ADD32(GPR_U32(ctx, rs), GPR_U32(ctx, rt)));` |
| XOR | `SET_GPR_U64(ctx, rd, GPR_U64(ctx, rs) ^ GPR_U64(ctx, rt));` |
| SLL | `SET_GPR_S32(ctx, rd, (int32_t)SLL32(GPR_U32(ctx, rt), sa));` |
| canonical NOP | `// NOP` |

Important exclusions:
- ADDIU to `$zero` emits a distinct NOP comment and should not be accepted
  until that exact case is separately covered.
- SLL with destination `$zero` may emit an empty statement; reject it for the
  first parser rather than guessing how empty output maps to an instruction.
- ADDU/XOR to `$zero` are emitted as setter calls; the Gekko2 AOT lowering
  must still preserve immutable GPR0.
- Do not infer support for ADD, DADDU, shifts other than SLL, loads/stores,
  branches, helper calls, COP0, FPU, VU or MMI from these examples.

The upstream `SET_GPR_S32/U64` macros write the low architectural 64-bit lane
while preserving the upper 64-bit lane. Gekko2's AOT translator also writes
`gpr[n].ud0` and preserves `ud1` for these operations. This is a useful
starting compatibility fact, not a general proof of the two runtimes' state
layouts or full register semantics.

## Lowering strategy

The safest first parser should be deliberately narrow and line-oriented,
not a general C++ parser and not a textual rewrite:

1. Select exactly one function by its generated identifier.
2. Require the exact upstream function parameter shape.
3. Require the preceding address-range comment and per-instruction raw-word
   comments. If comments are disabled, reject the source.
4. Require each instruction address to be aligned and contiguous, and require
   the following `ctx->pc` assignment to match that address.
5. Decode each raw word and accept only the five cases listed above, with the
   destination-zero exclusions stated above.
6. Reconstruct the expected upstream statement from the decoded fields and
   compare it to the actual emitted statement after whitespace normalization.
   Any mismatch, extra statement, label, conditional, nested block, runtime
   call, memory macro, preprocessor construct other than the known optional
   logging preamble, or missing final PC must reject the entire function.
7. Require the final fallthrough PC to equal the function end and the
   instruction count/range to agree.
8. Only then lower the verified raw words through
   `tools/ee_aot_generate.py` into Gekko2's existing AOT entry ABI.

This design uses the generated C++ as a semantic cross-check against the
original instruction words. It does not attempt to execute upstream C++ or
reinterpret `R5900Context*` as `ee_state_t*`. It also avoids trusting comments
alone: each comment's instruction must agree with the exact generated
statement that follows it.

## Existing Gekko2 backend compatibility

The current `tools/ee_aot_generate.py` already translates the corresponding
R5900 encodings for ADDIU, ADDU, XOR and SLL into Gekko2 AOT statements. The
generated entries are dispatched inside `ee_step_budget()` and participate
in the existing exact retirement/interleave accounting. Therefore the
smallest safe integration is a verified generated-C++ parser that produces
the existing AOT generator's input block representation. Do not create a
second scheduler or invoke the fail-closed bridge native-window API.

## Required parser tests before implementation is considered complete

- A pinned-format fixture with ADDIU, ADDU, XOR and SLL.
- Canonical NOP handling.
- Wrong function signature and wrong function selection.
- Missing/disabled instruction comments.
- Wrong instruction PC, noncontiguous PCs and wrong function end.
- Statement/opcode mismatch (including changed register indices/immediate).
- Destination-zero SLL and ADDIU-zero rejection.
- Branch, delay slot, memory operation, helper call, label, conditional,
  unknown macro and nested control-flow rejection.
- Multiple functions in one generated source file; only the selected function
  may be lowered.
- Generated AOT output compared against the interpreter for complete 128-bit
  GPR state, HI/LO, SA, PC/next-PC and exact retired count.

A fixture copied from a real upstream generator run is still required to
validate the entire source envelope end-to-end. The emitter source establishes
the templates, but it is not a substitute for executing the pinned upstream
binary and capturing its generated output.

## Status / next action

- Source format: audited from pinned upstream source.
- Actual generated game `.cpp` in public Gekko2 tree: none found.
- Parser / IR / lowering code: not implemented in this batch.
- Scheduler integration: existing Gekko2 AOT path is the target; upstream
  generated C++ is not executed.
- Host tests for this audit: not run; this batch is documentation-only.
- Wii build, Dolphin, physical Wii and FPS: not tested.

Claude's next task is to implement the strict parser and fixtures using this
document, then run the host CI. Do not broaden the accepted subset until the
first differential suite is green.
