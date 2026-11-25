# Branch map and working rules (2026-11-08)

One purpose per branch. Arrows show the only allowed direction of change.

| Branch | Role | Rule |
|---|---|---|
| `main` (`96030ac`) | Public baseline (BETA12.20 runtime + docs) | Do not merge anything without an owner decision. |
| `codex/crash-e3-local-repro` (`24b69fe`, PR #54) and `codex/crash-vu-review-download` (`11e193d`) | Crash E3 runtime fixes (`e7aae58`) and review artifacts | **Read-only.** Never reset, rebase, force-push or commit here. |
| `preserve/crash-e3-vu-runtime-20261108` (`24b69fe`) | Immutable pointer to the verified Crash snapshot | **Read-only.** Starting point for the integration line. |
| `codex/ps2recomp-bridge-batch-a-memory` (PR #58, Draft) | A1–A15 PS2Recomp bridge on top of `main` + preservation document | Frozen except fixes to A1–A15; it also carries the CRLF-only `third_party` commit `bf5c580`. |
| `codex/ps2recomp-a16-byte-half` | A16 (byte/halfword), based on A15 | Frozen after review. |
| `integration/crash-e3-plus-a15` | Crash runtime + A11–A16 + later gated batches; `third_party` identical to Crash | **Base for all further PS2Recomp work.** Created as merge `2179f6e`; since then it moves only by **fast-forward** to the newest fully gated batch branch (A17 → A18 → A19 so far), never by direct commits. |
| `codex/ps2recomp-a17-branches`, `-a18-calls`, `-a19-jumps`, … | One branch per batch, created from the previous integration state; each is an ancestor of the next | Touch only parser/tools/tests/fixtures/docs/workflows unless a runtime change is explicitly justified in the batch document. |

## Rules that keep the work clean

1. **Never edit Crash source** (`source/hw/vu*.c|h|inc`, `gif.c`, `vif.c`, `gs*.c`, `checkpoint.c`, `include/core/hw/vu_timing.h`)
   in a PS2Recomp batch. Every batch ends with a check that these files are byte-identical to
   `preserve/crash-e3-vu-runtime-20261108` (`tools/check_crash_files_unchanged.sh`).
2. **Runtime changes need their own commit and document**, never mixed with parser/fixture work.
3. **Generated or private data stays out**: no game ELFs, BIOS, ISOs; generated AOT lives in a
   private directory outside the checkout (`tools/ps2recomp_strict_parser.py --output-dir`).
4. **No formatting-only commits** in vendored code (`third_party/**`). Its blobs keep CRLF;
   locally add `third_party/** -text` to `.git/info/attributes` to stop checkouts showing them as modified.
5. **Order of gates before a push**: parser tests → AOT differential → bridge tests → devkitPPC build →
   linked PPC (Unicorn) → Crash gates (`tools/test_alpha_compat_host.py`, 92) — see
   `docs/CRASH-FIX-PRESERVATION-AND-A15-INTEGRATION.md`.
6. **Evidence labels**: host, Unicorn-modelled PPC, Dolphin and physical Wii are reported separately;
   only host and Unicorn exist so far.
7. **Rollback**: delete only the newest `codex/ps2recomp-aNN-*` branch; the integration line and the
   Crash branches are never rewritten. A merged batch is reverted with `git revert -m 1`.
