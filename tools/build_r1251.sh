#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
: "${DEVKITPRO:?Set DEVKITPRO to your devkitPro directory}"
: "${DEVKITPPC:?Set DEVKITPPC to the devkitPPC directory}"
# Separate object directories prevent flags from reusing the other mode's cache.
make BUILD=build-r1251-jit TARGET=PCSX2-Wii-R1251-DIV-LQ-Fix -j2
make BUILD=build-r1251-interpreter TARGET=PCSX2-Wii-R1251-Interpreter EXTRA_CFLAGS=-DPCSX2WII_JIT_DISABLE -j2
