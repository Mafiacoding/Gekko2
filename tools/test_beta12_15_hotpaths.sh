#!/usr/bin/env bash
# Focused host regression suite for EE cache, IOP tick grants, GS coherency.
# Unrelated CHD read code is stubbed for these three CPU-only tests. Do not
# use the stub for disc/CHD tests: full libchdr is linked in Wii builds.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p /tmp/gekko2-b1215-tests
sources=$(find source -name '*.c' ! -name main.c ! -name ppc_dynarec.c ! -name chd_reader.c)
for name in test_iop_budget_internal test_jit_cache_eviction_r1318 test_gs_hash_cache_r1368; do
    test_sources="$sources"
    if [[ "$name" == test_jit_cache_eviction_r1318 ]]; then
        test_sources=$(printf '%s\n' "$sources" | grep -v '/ee_jit.c$')
    fi
    echo "[HOST] $name"
    # shellcheck disable=SC2086
    gcc -O0 -w -ffunction-sections -fdata-sections -Wl,--gc-sections \
      -Iinclude -Isource "tests/$name.c" tests/support/chd_reader_stub_beta1215.c \
      $test_sources -o "/tmp/gekko2-b1215-tests/$name" -lm
    "/tmp/gekko2-b1215-tests/$name"
done
