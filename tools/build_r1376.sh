#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${DEVKITPRO:?Set DEVKITPRO}"
: "${DEVKITPPC:=$DEVKITPRO/devkitPPC}"
export DEVKITPPC
variant=${1:-B}
case "$variant" in
 A) graphics=-DGEKKO2_GX_RESIDENT_BLEND_DISABLE ;;
 B) graphics="-DGEKKO2_GX_NATIVE_ADDITIVE -DGEKKO2_GX_RESIDENT_FENCE" ;;
 *) echo 'Use A or B' >&2; exit 2 ;;
esac
make -j4 BUILD="build-r1376$variant" TARGET="Gekko2-R1376$variant" LIGHTREC_PILOT=1 EXTRA_CFLAGS="-DGEKKO2_EE_BLOCK_FAST -DGEKKO2_GOURAUD_EARLY_DEGENERATE -DGEKKO2_ARM_BETA -DGEKKO2_DSI_FASTMEM $graphics"
