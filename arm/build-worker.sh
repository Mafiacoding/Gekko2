#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
: "${DEVKITARM:?devkitARM required; this builds objects only, not an IOS installer}"
mkdir -p arm/build
"$DEVKITARM/bin/arm-none-eabi-gcc" -O2 -Wall -Wextra -mcpu=arm926ej-s -marm -mbig-endian -ffreestanding -Iinclude -c arm/worker.c -o arm/build/worker.o
"$DEVKITARM/bin/arm-none-eabi-gcc" -O2 -Wall -Wextra -mcpu=arm926ej-s -marm -mbig-endian -ffreestanding -Iinclude -c source/hw/ipu_csc.c -o arm/build/ipu_csc.o
