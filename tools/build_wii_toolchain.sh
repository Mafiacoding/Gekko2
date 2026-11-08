#!/usr/bin/env bash
# GPL-3.0-or-later. Reproducible clean Wii build with an official devkitPro
# install (see docs/WII-TOOLCHAIN-2026.md). Builds two configurations from a
# clean tree: the default Makefile build and the full AOT/DSI option build used
# by tools/build_r1389.sh, then runs the static MEM1 check and prints hashes.
#
#   DEVKITPRO=/opt/devkitpro bash tools/build_wii_toolchain.sh
#
# Optional pins (the build fails on mismatch):
#   GEKKO2_EXPECT_GCC=16.2.0  GEKKO2_EXPECT_LIBOGC=3.2.1
set -euo pipefail
cd "$(dirname "$0")/.."
: "${DEVKITPRO:?Set DEVKITPRO (e.g. /opt/devkitpro)}"
: "${DEVKITPPC:=$DEVKITPRO/devkitPPC}"
export DEVKITPRO DEVKITPPC
export PATH="$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$PATH"

gcc_version=$(powerpc-eabi-gcc -dumpfullversion)
ld_version=$(powerpc-eabi-ld --version | head -1)
header="$DEVKITPRO/libogc/include/ogc/libversion.h"
libogc_version=$(awk '/_V_MAJOR_/{a=$3}/_V_MINOR_/{b=$3}/_V_PATCH_/{c=$3}END{print a"."b"."c}' "$header")
echo "powerpc-eabi-gcc $gcc_version"
echo "$ld_version"
echo "libogc $libogc_version"
if [[ -n ${GEKKO2_EXPECT_GCC:-} && $gcc_version != "$GEKKO2_EXPECT_GCC" ]]; then
  echo "expected GCC $GEKKO2_EXPECT_GCC" >&2; exit 1
fi
if [[ -n ${GEKKO2_EXPECT_LIBOGC:-} && $libogc_version != "$GEKKO2_EXPECT_LIBOGC" ]]; then
  echo "expected libogc $GEKKO2_EXPECT_LIBOGC" >&2; exit 1
fi

jobs=${JOBS:-$(nproc 2>/dev/null || echo 2)}
log=$(mktemp)
trap 'rm -f "$log"' EXIT

echo "== default configuration (clean)"
rm -rf build-wii-default Gekko2-wii-default.elf Gekko2-wii-default.dol
make -j"$jobs" BUILD=build-wii-default TARGET=Gekko2-wii-default 2>&1 | tee "$log" | tail -3
echo "warnings: $(grep -c 'warning:' "$log")  errors: $(grep -c ' error' "$log" || true)"
python3 tools/verify_wii_memory.py Gekko2-wii-default.elf
sha256sum Gekko2-wii-default.dol

echo "== AOT/DSI option configuration (clean)"
rm -rf build-r1389-dsi Gekko2-R1389-DSI.elf Gekko2-R1389-DSI.dol
bash tools/build_r1389.sh 2>&1 | tee "$log" | tail -3
echo "warnings: $(grep -c 'warning:' "$log")"
sha256sum Gekko2-R1389-DSI.dol
