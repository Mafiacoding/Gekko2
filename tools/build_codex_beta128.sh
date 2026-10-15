#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${DEVKITPRO:=/opt/devkitpro}"
: "${DEVKITPPC:=$DEVKITPRO/devkitPPC}"
export DEVKITPRO DEVKITPPC
export PATH="$DEVKITPPC/bin:$DEVKITPRO/tools/bin:$PATH"
command -v powerpc-eabi-gcc
echo "== Gekko2 BETA12.21 Codex 128-bit experiment =="
powerpc-eabi-gcc --version | head -1

# Fresh, uniquely named build: never reuse stale BETA12.20 objects/flags.
python3 - <<'PY'
from pathlib import Path
p = Path('third_party/lightning/include/lightning.h')
if not p.exists():
    p.write_text(p.with_suffix('.h.in').read_text().replace('@MAYBE_INCLUDE_STDINT_H@', '#include <stdint.h>'))
PY
make -j4 BUILD=build-codex-beta128 TARGET=Gekko2-BETA12.21-CODEX-128BIT LIGHTREC_PILOT=1 \
 EXTRA_CFLAGS='-DGEKKO2_EE_CHAIN_RESIDENCY -DGEKKO2_SPEED_RETIRE -DGEKKO2_SPEED_STUB -DGEKKO2_GX_NATIVE_BIOS_TRI -ffp-contract=off -DGEKKO2_EE_AOT -DGEKKO2_VU_AOT -DGEKKO2_IOP_AOT -DGEKKO2_EE_BLOCK_FAST -DGEKKO2_GOURAUD_EARLY_DEGENERATE -DGEKKO2_ARM_BETA -DGEKKO2_DSI_FASTMEM -DGEKKO2_GX_NATIVE_ADDITIVE -DGEKKO2_GX_RESIDENT_FENCE -DGEKKO2_IDLE_SKIP'
python3 tools/verify_dol_r1352.py Gekko2-BETA12.21-CODEX-128BIT.dol Gekko2-BETA12.21-CODEX-128BIT.elf
python3 tools/verify_wii_memory.py Gekko2-BETA12.21-CODEX-128BIT.elf
strings Gekko2-BETA12.21-CODEX-128BIT.dol | grep -F 'R1389-SPEED-BETA12.21-CODEX-128BIT-EXPERIMENTAL'
if strings Gekko2-BETA12.21-CODEX-128BIT.dol | grep -F 'R1389-SPEED-BETA12.20-GX-ALPHA-FENCES'; then
  echo "ERROR: stale version string in linked binary" >&2
  exit 1
fi
echo "CHECKED BETA12.21 Codex 128-bit DOL"
