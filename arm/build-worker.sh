#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p arm/build
# Use devkitARM, or a separately installed Zig compiler for reproducible
# freestanding ARM926 big-endian builds. Never runs an IOS installer.
if [ -n "${DEVKITARM:-}" ]; then
 compiler="$DEVKITARM/bin/arm-none-eabi-gcc"
 set -- -mcpu=arm926ej-s -marm -mbig-endian
elif [ -n "${ZIG:-}" ]; then
 compiler="$ZIG"
 set -- cc -target armeb-freestanding-eabi -mcpu=arm926ej_s -marm
else
 echo 'Set DEVKITARM or ZIG to build the experimental ARM worker.' >&2
 exit 2
fi
"$compiler" "$@" -E -P -x c -DWORKER_BASE=${ARM_WORKER_BASE:-0x13700000} arm/worker.ld -o arm/build/worker.ld
"$compiler" "$@" -O2 -Wall -Wextra -ffreestanding -fno-builtin -fno-stack-protector -fno-unwind-tables -nostdlib -Iinclude arm/start.S arm/ios_syscalls.S arm/ios_service.c arm/worker.c source/hw/ipu_csc.c -Wl,-T,arm/build/worker.ld -Wl,--build-id=none -o arm/build/Gekko2-ARM-Worker.elf
