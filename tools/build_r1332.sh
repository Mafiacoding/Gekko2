#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${DEVKITPRO:?Set DEVKITPRO to the supplied SDK}"
: "${DEVKITPPC:?Set DEVKITPPC to the supplied compiler}"
make -j"${GEKKO2_BUILD_JOBS:-4}" TARGET=Gekko2-R1332-Optimized BUILD=build-r1332-optimized EXTRA_CFLAGS='-DGEKKO2_EE_BLOCK_FAST -DGEKKO2_GOURAUD_EARLY_DEGENERATE'
make -j"${GEKKO2_BUILD_JOBS:-4}" TARGET=Gekko2-R1332-Control BUILD=build-r1332-control EXTRA_CFLAGS='-DGEKKO2_EE_BLOCK_FAST -DGEKKO2_CACHE_LEGACY_DEFAULT -DGEKKO2_GOURAUD_EARLY_DEGENERATE'
python3 tools/verify_dol_r1330l.py Gekko2-R1332-Optimized.dol Gekko2-R1332-Optimized.elf
python3 tools/verify_dol_r1330l.py Gekko2-R1332-Control.dol Gekko2-R1332-Control.elf
