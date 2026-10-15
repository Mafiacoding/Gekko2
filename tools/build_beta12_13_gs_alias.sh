#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${DEVKITPRO:?Set DEVKITPRO to your devkitPro root}"
export DEVKITPPC="${DEVKITPPC:-$DEVKITPRO/devkitPPC}"
export PATH="$DEVKITPPC/bin:$DEVKITPRO/tools/bin:$PATH"
python3 - <<'PY'
from pathlib import Path
p=Path('third_party/lightning/include/lightning.h.in')
Path('third_party/lightning/include/lightning.h').write_text(p.read_text().replace('@MAYBE_INCLUDE_STDINT_H@','#include <stdint.h>'))
PY
# Preserve SPEED/DSI/GX flags from BETA12.12; O1 is for diagnostics only.
make -j4 BUILD=build-beta12-9 TARGET=Gekko2-SPEED-BETA12.13-GS-ALIAS LIGHTREC_PILOT=1 \
 CFLAGS='-g -O1 -Wall $(MACHDEP) $(INCLUDE) -DGEKKO -mrvl -mcpu=750 -meabi -mhard-float $(EXTRA_CFLAGS) $(CHD_FLAGS)' \
 EXTRA_CFLAGS='-DGEKKO2_SPEED_RETIRE -DGEKKO2_SPEED_STUB -DGEKKO2_GX_NATIVE_BIOS_TRI -ffp-contract=off -DGEKKO2_EE_AOT -DGEKKO2_VU_AOT -DGEKKO2_IOP_AOT -DGEKKO2_EE_BLOCK_FAST -DGEKKO2_GOURAUD_EARLY_DEGENERATE -DGEKKO2_ARM_BETA -DGEKKO2_DSI_FASTMEM -DGEKKO2_GX_NATIVE_ADDITIVE -DGEKKO2_GX_RESIDENT_FENCE -DGEKKO2_IDLE_SKIP'
python3 tools/verify_wii_memory.py Gekko2-SPEED-BETA12.13-GS-ALIAS.elf
python3 tools/verify_dol_r1352.py Gekko2-SPEED-BETA12.13-GS-ALIAS.dol Gekko2-SPEED-BETA12.13-GS-ALIAS.elf
