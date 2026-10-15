#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${DEVKITPRO:?Set DEVKITPRO to the recovered modern devkitPro SDK}"
: "${DEVKITPPC:=$DEVKITPRO/devkitPPC}"
export DEVKITPPC
export PATH="$DEVKITPRO/tools/bin:$PATH"
if [[ $# == 1 ]]; then
 profile=$1
 python3 tools/ee_aot_generate.py --profile "$profile" --max-blocks 512 --output source/core/recompiler/ee_aot_generated.inc --complex-output source/core/ee/ee_aot_complex_generated.inc
 python3 tools/vu_aot_generate.py --micro "$profile.vu0.bin" --micro "$profile.vu1.bin" --max-words 128 --output source/hw/vu_aot_generated.inc
 python3 tools/iop_aot_generate.py --profile "$profile" --output source/core/iop/iop_aot_generated.inc
fi
python3 - <<'PY'
from pathlib import Path
p=Path('third_party/lightning/include/lightning.h.in')
out=Path('third_party/lightning/include/lightning.h')
text=p.read_text().replace('@MAYBE_INCLUDE_STDINT_H@','#include <stdint.h>')
if not out.exists() or out.read_text()!=text:out.write_text(text)
PY
make -j4 BUILD=build-r1386-dsi TARGET=Gekko2-R1386-DSI LIGHTREC_PILOT=1 EXTRA_CFLAGS='-ffp-contract=off -DGEKKO2_EE_AOT -DGEKKO2_VU_AOT -DGEKKO2_IOP_AOT -DGEKKO2_EE_BLOCK_FAST -DGEKKO2_GOURAUD_EARLY_DEGENERATE -DGEKKO2_ARM_BETA -DGEKKO2_DSI_FASTMEM -DGEKKO2_GX_NATIVE_ADDITIVE -DGEKKO2_GX_RESIDENT_FENCE'
python3 tools/verify_wii_memory.py Gekko2-R1386-DSI.elf
