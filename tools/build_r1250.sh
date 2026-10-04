#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
: "${DEVKITPRO:?Set DEVKITPRO to your devkitPro directory}"
: "${DEVKITPPC:?Set DEVKITPPC to the devkitPPC directory}"
# Separate object directories prevent flags from reusing the other mode's cache.
make BUILD=build-r1250-jit TARGET=PCSX2-Wii-R1250-LQ-Fix -j2
make BUILD=build-r1250-interpreter TARGET=PCSX2-Wii-R1250-Interpreter EXTRA_CFLAGS=-DPCSX2WII_JIT_DISABLE -j2
