#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${DEVKITPRO:?Set DEVKITPRO to the supplied SDK}"
: "${DEVKITPPC:?Set DEVKITPPC to the supplied compiler}"
make -j"${GEKKO2_BUILD_JOBS:-4}" TARGET=Gekko2-R1331-Options BUILD=build-r1331-options EXTRA_CFLAGS='-DGEKKO2_EE_BLOCK_FAST -DGEKKO2_GOURAUD_EARLY_DEGENERATE'
make -j"${GEKKO2_BUILD_JOBS:-4}" TARGET=Gekko2-R1331-Reference BUILD=build-r1331-reference EXTRA_CFLAGS='-DGEKKO2_CODE_ARENA_DISABLE -DGEKKO2_GOURAUD_EARLY_DEGENERATE'
python3 tools/verify_dol_r1330l.py Gekko2-R1331-Options.dol Gekko2-R1331-Options.elf
python3 tools/verify_dol_r1330l.py Gekko2-R1331-Reference.dol Gekko2-R1331-Reference.elf
