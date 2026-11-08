# Crash E3 fix preservation and safe A15 integration plan

Date: 2026-11-08 · A15 branch `codex/ps2recomp-bridge-batch-a-memory` (PR #58, Draft, **not merged**)
Nothing on any Crash branch was reset, rebased, deleted or force-pushed. No Crash
source was copied into the A15 branch. All numbers below were measured in this
session (host Linux x86-64, devkitPPC r51 cross-builds, Unicorn-modelled PPC).

## 1. Verified Crash source

| Item | Value |
|---|---|
| Runtime source commit | `e7aae5872045c9ad8a554f3a2bbead04e398239f` — "Fix VU conditional branch bypass and delayed CLIP flags" (2026-10-19) |
| `codex/crash-e3-local-repro` / PR #54 head | `24b69fe5d46e7915f424e4bddcb833df778213bc` |
| `codex/crash-vu-review-download` head | `11e193d16553629055ff54515116c672183d0b68` (= crash-e3 + 2 commits of review downloads) |
| `downloads/crash-gx-handoff-2026-10-29` | `c58a7f116e521e727251c0fb8d9078fb0349f2cf` (unrelated history, inspected only) |
| `main` at snapshot | `96030acd388091ceced41be0a8c2036df6449c58` |
| Merge base crash-e3 ↔ main | `999df60c5e14502b289d63569f60fa164a8e2806` |
| Divergence main-only / crash-only | 5 / 104 (crash-e3); 5 / 106 (crash-vu). The 5 main commits are **documentation only** (PS2Recomp bridge handoffs). |
| A15 branch | `ecf3b562…` is 0 behind / 79 ahead of main; **no common runtime history with the Crash line beyond `999df60`**. |
| Documentation | `docs/CRASH-E3-VU-TIMING.md` (present on crash-e3 only) |

`e7aae58` is an ancestor of both Crash branches and of **neither** `main` nor the
A15 branch. Current `main`/A15 are at BETA12.20; the Crash fixes (and ~100 other
commits: GX, IPU, GIF, VIF, VU, telemetry, packaging) exist only on the Crash line.

## 2. Preservation artifacts

- **Remote branch** `preserve/crash-e3-vu-runtime-20261108` → `24b69fe…`
  (created through the GitHub API; contains `e7aae58`). Plain pointer, no new commits.
- **Annotated tags (local clone only)**: `preserve/crash-e3-runtime-e7aae58`,
  `preserve/crash-e3-local-repro-24b69fe`, `preserve/crash-vu-review-download-11e193d`,
  `preserve/main-at-snapshot-96030ac`. **Pushing tags was refused (HTTP 403 from the
  session's git proxy)**, so they are not on GitHub; they are inside the bundle below.
  Re-create them with `git tag -a <name> <sha>` and push from a normal clone.
- **Git bundle** `crash-e3-preservation.bundle` (33,234,772 bytes) — SHA-256
  `6b78441c9eba0e25ef602a363ad47641f1f72f8a2c1a2f54452b8535bc81f9b7`.
  `git bundle verify`: *complete history*. Restore test: `git clone --mirror` from the
  bundle; commits `e7aae58`, `24b69fe`, `11e193d`, `96030ac` all present (`git cat-file -t`
  = commit), `git fsck` clean, 982 commits reachable from the crash tag.
- **Patch archive** `crash-e3-patches.tar.gz` (1,250,421 bytes) — SHA-256
  `2eb4b59f8f1ad2457c6cafdebd1a1f2d8fa6482e2188fad4ff822a0e47416f1d`: 103
  `format-patch` files for `999df60..24b69fe` (the one merge commit has no patch),
  the single-commit patch of `e7aae58`, and the full diff `crash-e3-vs-999df60.diff`.
  The full diff applies cleanly to a checkout of `999df60`; the result equals the
  crash tree except for 23 `third_party` files whose blobs carry CRLF (see §7).
- **Existing review artifacts** (blobs of `codex/crash-vu-review-download`,
  `downloads/crash-vu-review/`), checksums re-verified and zip integrity tested:

  | File | Bytes | SHA-256 |
  |---|---|---|
  | `Gekko2-Alpha-Review-e7aae58-SD.zip` | 942,550 | `93fd1e15…350d` (matches `artifact-checksums.json`) |
  | `Gekko2-Crash-VU-ELF-Review-e7aae58.zip` | 7,023,595 | `eb7d42ed…d8db` (matches) |
  | `Gekko2-Claude-Handoff-24b69fe.zip` | 9,250,644 | `d980f37b…5462` (matches `.sha256`) |

  `artifact-checksums.json` also lists a source zip and a `from-f06bd78.patch` that are
  **not stored** in the branch (the README says to use the source commit archive); the
  bundle and patch archive above replace them.
- The files live in the session scratchpad (`…/scratchpad/crash-preservation/`); copy
  them to durable storage. The working tree of the A15 clone was clean at the start
  and at the end of the audit.

## 3. Crash-specific fixes (source level)

"In main" = symbol/test present in `origin/main`. "A15 touches" = file differs
between `origin/main` and the A15 head.

| # | Fix / behaviour protected | Commit(s) | Files and functions | Regression tests | In main | A15 touches file |
|---|---|---|---|---|---|---|
| 1 | VU0/VU1 delayed VI **conditional-branch bypass** (previous VI visible one issue cycle after integer ALU/MTIR/auto-update LQ/SQ; JR/JALR, flag writes, ILW unaffected) | `e7aae58` | `source/hw/vu.c` `vu_micro_words_timed`; `source/hw/vu_fmac_internal.h` `vu_vi_backup_dest`, `vu_vi_age`, `vu_vi_conditional_branch`; `include/core/hw/vu_timing.h` (`vi_age`, `vi_old`, `vi_reg`, `vi_marker`) | `tests/test_vu_vi_branch_latency.c` (six conditions, consecutive writes, stalls, JR exclusion); linked: `tools/verify_vu_timing_r1384.py` | **No** | No |
| 2 | **VI history across consecutive writes** to the same VI (earliest value kept, expires on real issue/stall clocks, cleared on stop, canonical VI restored after branch) | `e7aae58` | same as #1 (`timing->vi_old/vi_age` handling, `if(timing)timing->vi_age=0` on stop) | same | No | No |
| 3 | **CLIP/FCSET four-issue-clock retirement**, consecutive-CLIP history at issue, retired-only lower queries, same-pair upper CLIP priority, I-immediate, FSSET snapshots | `e7aae58` | `vu.c` `vu_micro_words_timed` (`delayed_clip`, `issued_clip`); `vu_fmac_internal.h` `vu_fmac_publish`, `vu_fmac_issue`, `vu_clip_issued` | `tests/test_vu_clip_latency.c` (includes an **actual checkpoint round trip**); linked `verify_vu_timing_r1384.py` | No | No |
| 4 | Flag **publication/query ordering**; ADD/SUB operand alignment | `e7aae58`, `9d14d74` | `vu_fmac_internal.h` `vu_fmac_arithmetic`, `vu_fmac_publish`; `vu.c` | `tests/test_vu_add_alignment.c`; linked `tools/verify_vu_add_alignment_linked.py` (512 programs / 3072 checks) | No | No |
| 5 | **Timed interpreter/AOT/JIT callback** behaviour: JIT pair path is skipped while FSSET/delayed CLIP is in flight; AOT/pipeline comparisons | `e7aae58`, `ee6306d` | `vu.c` (`vu_jit_try_pair` gate in `vu_micro_words_timed`), `vu_aot_*` | `tools/test_vu_masks_aot.py` (1024 comparisons) | No | No |
| 6 | **GIF PATH1 ownership / SIGNAL backpressure** (uncleared second SIGNAL blocks GIF DMA and VU1 producers; no clearing or forced progress) | `1d7c684` (+ `8553e2d` XGKICK failure paths, `d819c5c` transport diagnostics) | `source/hw/gif.c` `gif_try_process_quadwords`, `apply_ad_write`, `gif_init`; `include/core/hw/gif.h` `gif_signal_stall_count`; `source/main.c`; `source/hw/vu.c` `vu1_service_xgkick` | `tests/test_gif_signal_backpressure.c`, `tests/test_gs_signal.c`, `tests/test_xgkick_*` | **No** (`test_gs_signal.c` exists in main; the backpressure code/test do not) | No |
| 7 | **VIF1 DMA completion** / streaming DIRECT, interlocks, full-FIFO SQ retry | `80c4e04`, `bdd8230`, `c6a1a04` | `source/hw/vif.c` `vif_direct_drain`, `vif_process`, `vif_service_one`, `vif_accept_quadwords`; `source/core/ee/ee_core.c` `ee_mem_write128`, `ee_step_budget` | `tests/test_vif_direct_streaming.c`, `tests/test_vif.c`, `tests/test_vif_cpu_fifo.c`; linked `tools/verify_gs_vif_accuracy_linked.py` | No | **`ee_core.c`: yes** (see §4) |
| 8 | **EE/VU state & checkpoint serialization** | layout change in `e7aae58` only | `vu_timing.h` reuses former padding (24-byte queue slot and scoreboard offset 104 retained); `source/core/checkpoint.c` legacy-snapshot zeroing already in main (`5d3b3d3`) | `test_vu_clip_latency.c` checkpoint round trip; layout asserts in `test_vu_vi_branch_latency.c`; `test_ps2_mc_checkpoint`, `test_checkpoint_rspu2_r1340` | partial (legacy zeroing only) | No |
| 9 | **JIT/AOT invalidation and scheduler boundaries** around the above | various (`952cc59`, `5f33120`, `7791683`, `c6a1a04` change `ee_core.c`; `ee_aot.c` itself unchanged; one-line `ee_aot_generate.py` change adds C.F.S `fn 0x30`) | `source/core/ee/ee_core.c` (`ee_step_budget`, interleave quantum, `ee_mem_read/write8/16/32`), `source/core/recompiler/*` (+468/−48 lines) | `tools/test_alpha_compat_host.py` (92 gates), existing AOT tests | No | **`ee_core.c`, `ee_core.h`: yes** |

A similarly named test is **not** accepted as proof: each row's test was built and run
(§6). `docs/CRASH-E3-VU-TIMING.md` also records that the long black screen (software
float sine table) is *not* fixed by these commits; this plan does not change that.

## 4. Overlap of A15 with the Crash runtime

Runtime sources the A15 branch changes versus `main`:
`include/core/ee/ee_core.h` (+4: `ee_core_gpr_generation_bump` declaration),
`source/core/ee/ee_core.c` (+5: the function), `source/core/ee/ee_hle_thread.c`
(`load_context` calls the hook), and `source/core/recompiler/ps2recomp_bridge.c` /
`include/core/recompiler/ps2recomp_bridge.h` (fail-closed bridge, A4/A7). Everything
else on the A15 branch is documentation, tests, tools, fixtures, workflows and the
`third_party` line-ending commit (§7). **A15 changes none of**
`source/hw/vu.c`, `vu_fmac_internal.h`, `vu_lower_body.inc`, `gif.c`, `vif.c`, `gs.c`,
`gs_gx.c`, `gs_wii_output.c` or `include/core/hw/vu_timing.h`.

Files changed by both lines: `source/core/ee/ee_core.c`, `include/core/ee/ee_core.h`,
`README.md`, `STATUS.md`. `git merge-tree` of A15 + crash-e3: **only `CLAUDE.md` and
`README.md` conflict (documentation)**; `ee_core.c/.h` merge automatically (A15's
additions sit in unrelated places).

Semantic regression risks (not just textual):
1. **A16 memory helpers.** Commit `7791683` rewrites `ee_mem_read8/16/32` and
   `ee_mem_write8/16/32` (GS narrow accesses, IMR interrupts). A16 (LB/LBU/LH/LHU/SB/SH)
   lowers through exactly these helpers, so A16 must be validated on top of the merged
   tree before it can be trusted for Crash.
2. **Interleave contract.** The Crash line edits the EE interleave quantum in
   `ee_step_budget` (`ee_event_slots<first?…`) and adds CPU-FIFO stalls (`5f33120`,
   `c6a1a04`). The A8/A15 AOT contract (`ee_core_native_window_begin_aot`,
   `ee_core_interleave_account`, exact-once accounting) was re-tested on the merged
   tree (§6) and held.
3. **Generator.** The one-line `ee_aot_generate.py` change only adds `fn 0x30` to the
   COP1 family; the strict parser accepts no COP1, so lowering is unaffected.
4. No A15 code touches VU timing, GIF SIGNAL ownership, VIF DMA completion or
   checkpoint layout, so none of the protected fixes can be *regressed by A15 files*;
   the remaining risk is only in how the two lines are combined.

## 5. Integration plan

Principle: never replay the Crash fixes onto `main` in isolation and never copy the
whole Crash line into the A15 branch.

Why not a minimal cherry-pick: `git cherry-pick -n` onto `main` conflicts for
`e7aae58` (`vu.c`, `tools/test_alpha_compat_host.py` missing), `9d14d74`
(same test file) and `1d7c684` (`source/main.c`); only `c6a1a04` applies alone. The VU
fix depends on `8553e2d`, `d819c5c`, `ee6306d`, `9d14d74` (all touch `vu.c`).

Recommended sequence (each step stops on any red gate):

1. **Freeze.** Keep §2 artifacts; do not touch Crash branches/PR #54.
2. **Decide the base.** The Crash line is the runtime baseline for any real game
   work; `main`+A15 is the bridge baseline. Combine them on a *new* branch, e.g.
   `integration/crash-e3-plus-a15`, created from `preserve/crash-e3-vu-runtime-20261108`
   (so the Crash history stays byte-identical) and merge the A15 branch into it
   (`--no-ff`). Resolve `CLAUDE.md`/`README.md` by hand (A15 doc sections + the Crash
   handoff text). Do **not** merge into `main` or PR #58.
3. **Before the merge:** split PR #58's `bf5c580` (CRLF-only `third_party` commit) out
   — it is not needed for A15 and the Crash blobs keep CRLF (§7).
4. **Gates on the integration branch**, in this order: (a) `tools/test_alpha_compat_host.py`
   = 92 PASS; (b) `tools/test_vu_masks_aot.py` = 1024; (c) A11–A15: parser tests, AOT
   differential, bridge tests, hybrid test; (d) devkitPPC cross-build + `verify_wii_memory.py`;
   (e) linked PPC: `verify_vu_timing_r1384.py`, `verify_vu_add_alignment_linked.py`,
   `verify_gs_vif_accuracy_linked.py`, `ps2recomp_wii_ppc_check.py`.
5. **Then** continue PS2Recomp work (A16 …) on a branch based on that integration
   branch, so every new memory instruction is tested against the Crash-era
   `ee_mem_*` helpers.
6. Dolphin and a physical Wii remain required acceptance steps (§8).

Rollback: the integration branch is additive. To undo it, delete only that branch;
`preserve/*`, the Crash branches, PR #54 and `main` are untouched. To restore the Crash
runtime exactly: `git fetch origin preserve/crash-e3-vu-runtime-20261108 && git checkout -b
restore-crash FETCH_HEAD`, or `git clone crash-e3-preservation.bundle` (§2). To drop A15 from
an integration attempt: `git revert -m 1 <merge-sha>` (non-destructive) or reset the
*integration* branch to the preserve branch. Never force-push a Crash branch.

## 6. Test results (measured here)

| Gate | `24b69fe` (crash tip) | Trial merge A15 + crash-e3 |
|---|---|---|
| `tools/test_alpha_compat_host.py` (92 host gates, includes #1–#7 tests above) | **PASS 92**, exit 0 | **PASS 92**, identical test/exit list |
| `tools/test_vu_masks_aot.py` | PASS 1024 | PASS 1024 |
| devkitPPC r51 default Wii build | OK (114 warnings, 0 errors); MEM1 remaining 3,134,240 B | OK (114 warnings, 0 errors); MEM1 remaining 3,134,176 B |
| Linked PPC (Unicorn) `verify_vu_timing_r1384.py` | PASS (2 groups) | PASS (2 groups) |
| Linked PPC `verify_vu_add_alignment_linked.py` | PASS 512 programs / 3072 checks | PASS |
| Linked PPC `verify_gs_vif_accuracy_linked.py` | PASS (VIF0/1 SQ retry, MTBA, ENVX; 40 checks) | PASS |
| A11–A15 parser tests (55) / AOT differential (all fixtures) / bridge & hybrid tests | n/a | PASS (incl. 72 fault/device cases) |
| `ps2recomp_wii_ppc_check.py` (98+147+210+156 PPC comparisons) | n/a | PASS |

The trial merge existed only in a scratch worktree; it was not pushed or merged.
The Crash release notes tested devkitPPC r50/libogc 3.1.0; these runs are r51/libogc 3.2.1.

**Missing/failing tests:** `tools/verify_vu_pipeline_internal.py` is obsolete on the
Crash runtime (references removed `vu_jit_try_block`, as its own doc states); the
`test_ee_aot`/`test_vu_aot`/`test_iop_aot` C tests need private generated fixtures; the
other ~40 pre-existing non-passing host tests listed in `docs/WII-TOOLCHAIN-2026.md`
are unrelated to these fixes. No test exists for the *combination* of Crash VU timing
with a PS2Recomp-lowered function (nothing lowers VU/COP2 code yet).

## 7. Third-party changes in PR #58

PR #58 changes 23 files under `third_party/libchdr` and `third_party/lightrec/tlsf`
in a single commit, `bf5c580` ("normalize CRLF line endings"): **2569 insertions /
2569 deletions, zero differences with `--ignore-cr-at-eol`**. Cause: the repository's
`.gitattributes` requires `eol=lf` for `*.c`/`*.h`, but the blobs on `main` and on the
Crash branches still contain CRLF, so the working tree shows them modified after every
checkout. This is formatting noise, not a code change; it should **not** ride along with
A15 or the Crash integration. Recommendation: remove it from the PR (new branch without
`bf5c580`, or a dedicated one-purpose PR) and keep upstream-vendored sources
untouched. No Lightrec/libchdr logic differs.

## 8. Evidence classes (kept separate)

- **Host (native x86-64):** all numbers in §6 except the linked rows.
- **Linked PPC, Unicorn-modelled:** the three `verify_*_linked/…` rows and
  `ps2recomp_wii_ppc_check.py`. Platform allocation/cache services are mocked; no GX/VI
  hardware, no cycle counts.
- **Dolphin:** none for either line.
- **Physical Wii:** none for this session. The Crash document records no Dolphin/Wii
  execution of its batch either; observed Crash progression there was from the host
  software renderer with the owner's private disc (not in any repository).

## 9. Integration branch (created 2026-11-08)

`integration/crash-e3-plus-a15` = merge commit `2179f6e` with first parent
`preserve/crash-e3-vu-runtime-20261108` (`24b69fe`, Crash runtime `e7aae58`) and second
parent `codex/ps2recomp-a16-byte-half` (`91aecb4`: A11–A16, toolchain update, §1–§8 of
this document). Not merged into `main` or PR #58; PR #54 and all Crash branches untouched.

- Hand-resolved: `CLAUDE.md` (integration header + PS2Recomp block + both Crash handoffs) and
  `README.md` (Crash README + r51 toolchain note).
- `third_party` is **byte-identical to the Crash branch**: the CRLF-only commit `bf5c580` is not
  carried (§7 recommendation applied here only; PR #58 itself still contains it).
- Runtime difference to the Crash tip: only A15's `ee_core_gpr_generation_bump` (`ee_core.c/.h`,
  `ee_hle_thread.c`) and the fail-closed bridge files. `vu.c`, `vu_fmac_internal.h`, `vu_timing.h`,
  `gif.c`, `vif.c`, `gs.c`, `gs_gx.c`, `checkpoint.c`, `ee_aot.c` are **identical** to the Crash tip.

Gates run on this exact commit (host + Unicorn-modelled PPC, devkitPPC r51):

| Gate | Result |
|---|---|
| `test_alpha_compat_host.py` (92 gates incl. VU VI-branch/CLIP, SIGNAL backpressure, VIF FIFO) | PASS 92 |
| `test_vu_masks_aot.py` | PASS 1024 |
| Strict parser tests (62), `test_ee_aot_inputs`, AOT differential A11–A16 (source-only and ELF-bound), hybrid test (512), bridge/bridge_window/residency/lifecycle | PASS |
| Wii default build, `verify_wii_memory.py` | OK, 114 warnings / 0 errors, MEM1 remaining 3,134,176 B |
| Linked PPC: `verify_vu_timing_r1384`, `verify_vu_add_alignment_linked`, `verify_gs_vif_accuracy_linked` | PASS |
| `ps2recomp_wii_ppc_check.py` (98 / 147 / 210 / 164 / 229 comparisons) | PASS |

Still not done: Dolphin and physical Wii for this combination; real Crash disc runs (private,
not available here); the ~40 pre-existing non-passing host tests are unchanged. Rollback: delete only
this branch; restore with `git branch -f` is **never** needed for the Crash branches.
