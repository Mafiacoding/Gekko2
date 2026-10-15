#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${DEVKITPRO:?Set DEVKITPRO}"
export DEVKITPPC="${DEVKITPPC:-$DEVKITPRO/devkitPPC}"
export PATH="$DEVKITPPC/bin:$DEVKITPRO/tools/bin:$PATH"
python3 - <<'PY'
from pathlib import Path
src=Path('third_party/lightning/include/lightning.h.in')
dst=Path('third_party/lightning/include/lightning.h')
dst.write_text(src.read_text().replace('@MAYBE_INCLUDE_STDINT_H@','#include <stdint.h>'))
PY
make -j4 BUILD=build-beta12-5 TARGET=Gekko2-SPEED-BETA12.5-FILEIO-TRACE LIGHTREC_PILOT=1 \
 EXTRA_CFLAGS='-DGEKKO2_SPEED_RETIRE -DGEKKO2_SPEED_STUB -DGEKKO2_GX_NATIVE_BIOS_TRI -ffp-contract=off -DGEKKO2_EE_AOT -DGEKKO2_VU_AOT -DGEKKO2_IOP_AOT -DGEKKO2_EE_BLOCK_FAST -DGEKKO2_GOURAUD_EARLY_DEGENERATE -DGEKKO2_ARM_BETA -DGEKKO2_DSI_FASTMEM -DGEKKO2_GX_NATIVE_ADDITIVE -DGEKKO2_GX_RESIDENT_FENCE -DGEKKO2_IDLE_SKIP'
python3 tools/verify_wii_memory.py Gekko2-SPEED-BETA12.5-FILEIO-TRACE.elf
python3 tools/verify_dol_r1352.py Gekko2-SPEED-BETA12.5-FILEIO-TRACE.dol Gekko2-SPEED-BETA12.5-FILEIO-TRACE.elf
