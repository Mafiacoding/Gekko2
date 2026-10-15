#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${DEVKITPRO:?Set DEVKITPRO}"
: "${DEVKITPPC:=$DEVKITPRO/devkitPPC}"
export DEVKITPPC
if [ "$#" != 1 ]; then
  echo 'Usage: bash tools/build_ee_aot.sh PRIVATE_PROFILE_JSON' >&2
  exit 2
fi
python3 tools/ee_aot_generate.py --profile "$1" --max-blocks "${EE_AOT_MAX_BLOCKS:-512}" --output source/core/recompiler/ee_aot_generated.inc --complex-output source/core/ee/ee_aot_complex_generated.inc
python3 tools/vu_aot_generate.py --micro "$1.vu0.bin" --micro "$1.vu1.bin" --output source/hw/vu_aot_generated.inc
python3 tools/iop_aot_generate.py --profile "$1" --output source/core/iop/iop_aot_generated.inc
make -j4 BUILD=build-r1379-aot3 TARGET=Gekko2-R1379-AOT3 LIGHTREC_PILOT=1 EXTRA_CFLAGS='-ffp-contract=off -DGEKKO2_EE_AOT -DGEKKO2_VU_AOT -DGEKKO2_IOP_AOT -DGEKKO2_EE_BLOCK_FAST -DGEKKO2_GOURAUD_EARLY_DEGENERATE -DGEKKO2_ARM_BETA -DGEKKO2_DSI_FASTMEM -DGEKKO2_GX_NATIVE_ADDITIVE -DGEKKO2_GX_RESIDENT_FENCE'
python3 tools/verify_wii_memory.py Gekko2-R1379-AOT3.elf
