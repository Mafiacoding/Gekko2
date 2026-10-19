#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${DEVKITPRO:?}"; : "${DEVKITPPC:?}"
export PATH="$DEVKITPPC/bin:$DEVKITPRO/tools/bin:$PATH"
python3 - <<'PY'
from pathlib import Path
p = Path('third_party/lightning/include/lightning.h')
if not p.exists():
    p.write_text(p.with_suffix('.h.in').read_text().replace('@MAYBE_INCLUDE_STDINT_H@', '#include <stdint.h>'))
PY
variant="${CHAIN_RESIDENCY:-1}"
case "$variant" in
 0) target=Gekko2-BETA12.24-Trace; build=build-beta12-24-control; chain_flag='' ;;
 1) target=Gekko2-BETA12.24-Trace-Chain; build=build-beta12-24-chain; chain_flag='-DGEKKO2_EE_CHAIN_RESIDENCY' ;;
 *) echo 'CHAIN_RESIDENCY must be 0 or 1' >&2; exit 2 ;;
esac
make -j4 BUILD="$build" TARGET="$target" LIGHTREC_PILOT=1 \
 EXTRA_CFLAGS="-DGEKKO2_GAME_TRACE -DGEKKO2_SPEED_RETIRE -DGEKKO2_SPEED_STUB -DGEKKO2_GX_NATIVE_BIOS_TRI -ffp-contract=off -DGEKKO2_EE_AOT -DGEKKO2_VU_AOT -DGEKKO2_IOP_AOT -DGEKKO2_EE_BLOCK_FAST -DGEKKO2_GOURAUD_EARLY_DEGENERATE -DGEKKO2_ARM_BETA -DGEKKO2_DSI_FASTMEM -DGEKKO2_GX_NATIVE_ADDITIVE -DGEKKO2_GX_RESIDENT_FENCE -DGEKKO2_IDLE_SKIP $chain_flag"
python3 tools/verify_dol_r1352.py "$target.dol" "$target.elf"
python3 tools/verify_wii_memory.py "$target.elf"
