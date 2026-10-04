# PCSX2-Wii R1272

R1272 adds a bounded straight-line VU1 block backend on the R1271 native pair baseline. It is not the full EE/IOP/VU JIT port. R1271 remains saved as fallback.

## Implementation and limits

ppc_dynarec_translate_vu_block builds one native function containing two to eight upper/lower pairs. It never calls the interpreter between them. ABI arguments are retained across each pair and callee-saved registers/stack restored. Translation is transactional; failed translation leaves the caller buffer unchanged.

vu_jit_try_block owns at most 32 native blocks, keyed by micro pointer, PC, mask and every compiled raw word. Every entry is revalidated before execution; replacements free the previous code. Transient allocations/finalization failures remain retryable. Local data/micro overlap is rejected so stores cannot mutate future words during a block. The pair and single-operation fallbacks remain available.

Blocks exclude lower branches/jumps and upper E/D/T/M/other control flags. I-immediate is compiled. Pending branch/E delay, budgets below two, or unsupported instructions force fallback. TPC and retired-pair count advance by the exact executed length. No GIF/Q/EFU helper or side-effect boundary is crossed. The previous sequential upper-before-lower model remains; real PS2 VU pipeline/float accuracy is still partial.

VU1 MSCAL and MSCNT use blocks only if the first two pairs at entry are candidates; otherwise the entire invocation uses the prior pair loop. This avoids repeated scanning on branch-heavy entry paths but can miss later optimizable tails. VU0 still uses the pair backend; the generic block compiler/runtime is not yet wired into VU0. There is no block linking/register allocator yet.

## Verification

- 9,760 generated PPC block cases, lengths two through eight, 95 upper and 23 non-branch lower encodings, finite arithmetic, raw MIN/MAX/conversion inputs, I-immediate, local memory/wrapping, register aliases, ABI. Compile decline tested for size, control flag, branch and unsupported boundaries.
- 180/180 native regressions.
- Both actual Wii ELF builds pass inherited EE/IOP/VU/pair checks. Enabled JIT block tests cover retirement, E/branch/pending-delay guards, budget clamp (including cached longer blocks), word replacement, no partial execution, micro wrap, data/code alias rejection and transient retry.
- Repeated arithmetic testing also exposed a pre-existing PPC/host NaN-sign difference after overflow. General PS2 VU float normalization/saturation is open. Finite block tests do not prove general NaN/overflow arithmetic accuracy; raw MIN/MAX/conversion tests remain separate.

## PPC instruction counts (not FPS)

Three identical warm samples per workload, R1271 versus R1272, with identical final VF/VI/ACC/local memory:

| Workload | Retired pairs | R1271 PPC instructions | R1272 PPC instructions | Change |
| --- | ---: | ---: | ---: | ---: |
| Branch-heavy loop | 302 | 57,137 | 57,170 | +0.058% |
| Straight 64-pair program plus E delay | 66 | 11,360 | 7,018 | -38.222% |
| Eight straight pairs, branch and delay loop | 202 | 35,517 | 24,930 | -29.808% |

The initial per-pair candidate draft regressed the branch loop to 62,182 instructions (+8.830%); it was replaced by invocation-entry selection. These are synthetic real-PPC instruction counts, not hardware cycles, whole-emulator speed, BIOS response latency or Wii FPS. No new Tekken frame or FPS result is claimed.

## Build and reproduce

Use tools/build_r1272.sh with supplied devkitPPC/libogc and DEVKITPRO/DEVKITPPC set, serialized -j1. verify_jit_vu_block_r1272.py requires Unicorn. verify_ppc_block_r1272.py takes ELF and --nm. compare_ppc_vu_branch_r1272.py, compare_ppc_vu_straight_r1272.py and compare_ppc_vu_mixed_r1272.py take before ELF, after ELF and --nm.

## Open work

VU0 block integration, multi-block linking/register allocation, full VU MAC/status/Q/P/EFU/random and parallel hazards. EE/IOP blocks must retain per-instruction Count/timer/IRQ/SIF behavior and eight-to-one core interleave. EE/IOP declined paths and exception/load-delay accuracy remain partial. BIOS/game/private RAM checkpoints are excluded from public artifacts. Claude patch/diff are identical cumulative unified patches against original pcsx2-wii-full-debug(1).zip, with exact apply/byte comparison and ZIP CRC/SHA verification.

## Authentic BIOS navigation

Native interpreter continuation from the R1271 private checkpoint: Cross opens Browser, release, Circle returns to OSDSYS, release. EE 1,679,274,722 to 1,800,149,058, another 120,874,336 instructions; halt=0. Browser/menu screenshots inspected. No guest RAM/PC/menu flags forced. This is a checkpoint continuation, not a new cold boot or a generated-PPC BIOS run.
