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
task_build=build-r1388-dsi
task_target=Gekko2-R1388-DSI
task_gpu_flag=
if [[ ${GEKKO2_NATIVE_GPU_TEST:-0} == 1 ]]; then
 task_build=build-r1388-gpu-test
 task_target=Gekko2-R1388-GPU-TEST
 task_gpu_flag=-DGEKKO2_GX_NATIVE_BIOS_TRI
fi
make -j4 BUILD="$task_build" TARGET="$task_target" LIGHTREC_PILOT=1 EXTRA_CFLAGS="$task_gpu_flag -ffp-contract=off -DGEKKO2_EE_AOT -DGEKKO2_VU_AOT -DGEKKO2_IOP_AOT -DGEKKO2_EE_BLOCK_FAST -DGEKKO2_GOURAUD_EARLY_DEGENERATE -DGEKKO2_ARM_BETA -DGEKKO2_DSI_FASTMEM -DGEKKO2_GX_NATIVE_ADDITIVE -DGEKKO2_GX_RESIDENT_FENCE"
python3 tools/verify_wii_memory.py "$task_target.elf"
