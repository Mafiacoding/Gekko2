# PS2Recomp strict parser — Batch A14: ELF identity binding and private build

Date: 2026-11-08 · Branch `codex/ps2recomp-bridge-batch-a-memory` (PR #58, Draft)
Pinned upstream: `e5636981a1c9de368f6b71e819c3dc188dfedbf4`

## Why

A9 required the ELF identity, upstream commit, generated-source hash and
selected function address to be pinned, and A13 left one inferred fact: an
elided NOP delay slot. A14 closes both by cross-checking the generated C++
against the ELF it was generated from, and adds the private-output pipeline
that the existing hybrid tool already uses for guest-derived code.

## `bind_to_elf(result, elf)`

Rejects unless all hold:

1. The ELF is a little-endian MIPS ELF32 `ET_EXEC` that `ee_aot_generate.load_segments`
   accepts; unchanged while read.
2. The function range `[start, end)` has exactly one executable PT_LOAD backing.
3. The parsed words equal the ELF words at that range. The first differing
   address is reported.
4. If the source elided the delay slot, the ELF word at the slot must be a
   guest NOP exactly as upstream's `isGuestNop` defines it
   (`sll $zero,$zero,0` with any rs bits, or `addiu $zero,$zero,0`); that real
   word replaces the inferred `0`, so the lowered AOT block is byte-for-byte the
   guest code (`slot_source: "elf"`).
5. If a FUNC symbol exists at `start`, its size equals `end - start`
   (`elf_symbol_checked`). A missing symbol is recorded, not assumed.

The bound result carries the ELF SHA-256.

## `write_private_build` / CLI

```sh
python3 tools/ps2recomp_strict_parser.py --source ps2_recompiled_functions.cpp \
    --function NAME_0xADDR [--function …] --elf game.elf --output-dir /private/dir
```

- `--output-dir` requires `--elf` (unbound builds are only available via the
  development `--output`), must be outside the checkout, must not exist, is
  created `0700` with `0600` files.
- It writes `ee_aot_generated.inc`, `core/ee/ee_aot_complex_generated.inc`
  (same layout as `experiments/ps2recomp-wii/hybrid_compile.py`) and
  `report.private.json`: parser version, upstream commit, per-function range,
  terminal kind, slot source, source/ELF/AOT SHA-256, `wii_verified: false`.
- stdout carries only counts and hashes — no guest addresses or opcodes. A
  failing function leaves no directory behind.

## Evidence (host-native)

- 48 parser tests OK. New: all fixtures bind to their ELFs; every function word
  of the six A13 functions flipped one bit at a time is rejected (except a
  still-NOP elided slot); non-NOP elided slots rejected, all three NOP forms
  accepted; symbol-size mismatch; non-ELF, truncated, `ET_DYN`, big-endian,
  missing file, range outside/straddling a segment; private directory
  permissions, refusal inside the checkout, no overwrite; CLI exit codes and
  stdout hygiene.
- The AOT differential now runs every fixture twice, source-only and
  ELF-bound: A11 240+240, A12 360+360, A13 720+720 interpreter comparisons.
  Bound mode asserts parsed words == guest memory for all functions, so the
  `nop_alias` function is now executed natively end to end (source-only mode
  still covers the live-source fallback).

## Limits / not claimed

- Binding proves the generated function matches *this ELF's bytes*; it does not
  prove the ELF is a legitimate title or that the upstream tool was run on it.
  Hashes in the private report are the audit trail.
- Still single straight-line block + terminal `jr $ra`; entry at function start.
- No Wii cross-build (no devkitPPC in this container), no Dolphin/hardware, no
  performance claim.

## Next (A15 candidates)

- Compile an ELF-bound private include with devkitPPC (`tools/build_ee_aot.sh`
  style) and run the existing linked-PPC tests.
- Add loads/stores through the existing `ee_mem_*` helpers (one width at a time,
  with MMIO/TLB differential cases), then conditional branches inside a function.
