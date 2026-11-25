#!/usr/bin/env bash
# GPL-3.0-or-later. Fail if any protected Crash E3 source differs from the preserved snapshot.
# Usage: bash tools/check_crash_files_unchanged.sh [REF]   (default: preserve branch, fetched)
set -euo pipefail
cd "$(dirname "$0")/.."
ref=${1:-origin/preserve/crash-e3-vu-runtime-20261108}
git rev-parse --verify -q "$ref^{commit}" >/dev/null || { echo "unknown ref $ref (git fetch origin first)" >&2; exit 2; }
files=(source/hw/vu.c source/hw/vu_fmac_internal.h source/hw/vu_lower_body.inc include/core/hw/vu_timing.h
       source/hw/gif.c include/core/hw/gif.h source/hw/vif.c include/core/hw/vif.h
       source/hw/gs.c source/hw/gs_gx.c source/hw/gs_wii_output.c source/core/checkpoint.c
       source/core/recompiler/ee_aot.c source/core/recompiler/ee_jit.c source/core/recompiler/ppc_dynarec.c)
bad=0
for f in "${files[@]}"; do
  if ! git diff --quiet "$ref" HEAD -- "$f"; then echo "CHANGED  $f"; bad=1; else echo "same     $f"; fi
done
[[ $bad == 0 ]] && echo "OK: protected Crash files identical to $ref" || { echo "FAIL: protected Crash files differ" >&2; exit 1; }
