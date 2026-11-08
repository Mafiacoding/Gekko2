#!/usr/bin/env bash
# GPL-3.0-or-later. Build a Wii ELF containing the ELF-bound PS2Recomp AOT
# blocks from a private directory written by
#   tools/ps2recomp_strict_parser.py --elf GAME.elf --output-dir PRIVATE_DIR
# The tracked tree is never modified: sources are copied to a scratch tree, the
# private includes are dropped in there, and the ELF is written into PRIVATE_DIR.
#
#   DEVKITPRO=/opt/devkitpro bash tools/build_wii_private_aot.sh PRIVATE_DIR
set -euo pipefail
cd "$(dirname "$0")/.."
root=$PWD
: "${DEVKITPRO:?Set DEVKITPRO}"
: "${DEVKITPPC:=$DEVKITPRO/devkitPPC}"
export DEVKITPRO DEVKITPPC
export PATH="$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$PATH"
[[ $# == 1 && -f $1/ee_aot_generated.inc && -f $1/core/ee/ee_aot_complex_generated.inc ]] || {
  echo "usage: $0 PRIVATE_DIR (from ps2recomp_strict_parser.py --output-dir)" >&2; exit 2; }
private=$(cd "$1" && pwd)
case $private/ in "$root"/*) echo "private directory must be outside the checkout" >&2; exit 2;; esac
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT
cp -a Makefile include source third_party tools arm "$scratch"/
cp "$private/ee_aot_generated.inc" "$scratch/source/core/recompiler/ee_aot_generated.inc"
cp "$private/core/ee/ee_aot_complex_generated.inc" "$scratch/source/core/ee/ee_aot_complex_generated.inc"
cd "$scratch"
python3 - <<'PY'
from pathlib import Path
p=Path('third_party/lightning/include/lightning.h.in')
out=Path('third_party/lightning/include/lightning.h')
text=p.read_text().replace('@MAYBE_INCLUDE_STDINT_H@','#include <stdint.h>')
if not out.exists() or out.read_text()!=text:out.write_text(text)
PY
make -j"${JOBS:-$(nproc)}" BUILD=build-private TARGET=Gekko2-private-aot LIGHTREC_PILOT=1 \
  EXTRA_CFLAGS="-ffp-contract=off -DGEKKO2_EE_AOT -DGEKKO2_VU_AOT -DGEKKO2_IOP_AOT -DGEKKO2_EE_BLOCK_FAST -DGEKKO2_GOURAUD_EARLY_DEGENERATE -DGEKKO2_ARM_BETA -DGEKKO2_DSI_FASTMEM -DGEKKO2_GX_NATIVE_ADDITIVE -DGEKKO2_GX_RESIDENT_FENCE" >make.log 2>&1 || { tail -30 make.log >&2; exit 1; }
python3 tools/verify_wii_memory.py Gekko2-private-aot.elf
install -m 600 Gekko2-private-aot.elf "$private/Gekko2-private-aot.elf"
sha256sum "$private/Gekko2-private-aot.elf"
