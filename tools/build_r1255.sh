#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
: "${DEVKITPRO:?Set DEVKITPRO to your devkitPro directory}"
: "${DEVKITPPC:?Set DEVKITPPC to the devkitPPC directory}"
# Separate object directories prevent flags from reusing the other mode's cache.
make BUILD=build-r1255-jit TARGET=PCSX2-Wii-R1255-Diskless-JIT EXTRA_CFLAGS="-DPCSX2WII_DISKLESS -DPCSX2WII_FAST" -j2
make BUILD=build-r1255-interpreter TARGET=PCSX2-Wii-R1255-Diskless-Interpreter EXTRA_CFLAGS="-DPCSX2WII_JIT_DISABLE -DPCSX2WII_DISKLESS -DPCSX2WII_FAST" -j2
make BUILD=build-r1255-disc-interpreter TARGET=PCSX2-Wii-R1255-Disc-Interpreter EXTRA_CFLAGS="-DPCSX2WII_JIT_DISABLE -DPCSX2WII_FAST" -j2
