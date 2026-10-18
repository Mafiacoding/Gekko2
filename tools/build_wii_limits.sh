#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${DEVKITPRO:?}"; : "${DEVKITPPC:?}"
out="${LIMITS_OUTPUT:-/tmp/gekko2-wii-limits}"
mkdir -p "$out"
commit=$(git rev-parse HEAD)
cc="$DEVKITPPC/bin/powerpc-eabi-gcc"
flags=(-DGEKKO -mrvl -mcpu=750 -meabi -mhard-float -O2 -g -std=gnu11 -Wall -Wextra -Werror -ffp-contract=off -fno-builtin-memcpy -ffunction-sections -fdata-sections -I"$DEVKITPRO/libogc/include" -Iapps/wii_limits)
"$cc" "${flags[@]}" "-DLIMITS_COMMIT=\"$commit\"" -c apps/wii_limits/main.c -o "$out/main.o"
"$cc" "${flags[@]}" -c apps/wii_limits/kernels.c -o "$out/kernels.o"
"$cc" -mrvl -mcpu=750 -meabi -mhard-float -L"$DEVKITPRO/libogc/lib/wii" -specs="$DEVKITPRO/libogc/share/rvl.specs" -Wl,-Map,"$out/Wii-Limits.map" "$out/main.o" "$out/kernels.o" -lfat -lwiiuse -lbte -logc -lm -o "$out/Wii-Limits.elf"
python3 tools/elf2dol.py "$out/Wii-Limits.elf" "$out/Wii-Limits.dol"
python3 tools/verify_dol_r1352.py "$out/Wii-Limits.dol" "$out/Wii-Limits.elf" > "$out/dol.json"
python3 tools/verify_wii_memory.py "$out/Wii-Limits.elf" > "$out/memory.json"
"$cc" --version > "$out/compiler.txt"
printf '%s\n' "$commit" > "$out/build-commit.txt"
printf '%s\n' "${flags[*]}" > "$out/flags.txt"
