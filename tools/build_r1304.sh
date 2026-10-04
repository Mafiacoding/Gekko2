#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
: "${DEVKITPRO:?Set DEVKITPRO}"
: "${DEVKITPPC:?Set DEVKITPPC}"
make BUILD=build-r1304-ui TARGET=Gekko2-R1304-Menu-Interpreter EXTRA_CFLAGS="-DPCSX2WII_JIT_DISABLE -DPCSX2WII_FAST" -j1
make BUILD=build-r1304-ui-jit TARGET=Gekko2-R1304-Menu-JIT EXTRA_CFLAGS="-DPCSX2WII_FAST" -j1
