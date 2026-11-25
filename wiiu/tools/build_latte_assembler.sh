#!/usr/bin/env bash
# GPL-3.0-or-later. Builds decaf-emu's latte-assembler (GX2 shader assembler, GPL-3.0) on Linux without SDL,
# Qt, Vulkan or glslang. Only the "assemble" command is needed (Latte assembly -> .gsh); "compile" (GLSL via the
# Windows AMD Shader Analyzer) is stubbed out.
#
# Usage: wiiu/tools/build_latte_assembler.sh <work-dir>
# Result: <work-dir>/la-build/latte-assembler. Put it on PATH; wiiu/tools/build_all.sh then builds present.gsh.
#
# Verified: wiiu/gx2/shaders/present.{vsh,psh} assemble to a file byte-identical to the present.gsh built by the
# owner with the official Windows artifact.
set -euo pipefail
WORK=${1:?work directory}
DECAF_COMMIT=e6c528a20a41c34e0f9eb91dd3da40f119db2dee
mkdir -p "$WORK"
cd "$WORK"
if [ ! -d decaf-emu ]; then
  git clone https://github.com/decaf-emu/decaf-emu.git
fi
git -C decaf-emu checkout -q "$DECAF_COMMIT"
git -C decaf-emu submodule update --init --depth 1 \
  libraries/excmd libraries/cpp-peglib libraries/fmt libraries/gsl-lite libraries/spdlog libraries/cnl
D=$PWD/decaf-emu
B=$PWD/la-build
rm -rf "$B"
mkdir -p "$B"
cd "$B"
cp "$D"/tools/latte-assembler/src/*.cpp "$D"/tools/latte-assembler/src/*.h .
rm glsl_compiler.cpp
cat > glsl_stub.cpp <<'EOF'
#include "glsl_compiler.h"
std::string compileShader(std::string, std::string, ShaderType) { return {}; }
EOF
python3 - "$D" <<'EOF'
import sys
d = sys.argv[1]
s = open('main.cpp').read()
s = s.replace('#include <glslang/Public/ShaderLang.h>', '')
s = s.replace('glslang::InitializeProcess();', '').replace('glslang::FinalizeProcess();', '')
open('main.cpp', 'w').write(s)
# The pinned cpp-peglib returns comment tokens without the leading ';', the key/value regex required it.
s = open('gfd.cpp').read()
s = s.replace('";[[:space:]]*\\\\$', '";?[[:space:]]*\\\\$')
open('gfd.cpp', 'w').write(s)
g = open(d + '/tools/latte-assembler/resources/grammar.txt').read()
open('grammar.h', 'w').write('const char *LatteGrammar = R"(\n' + g + ')";\n')
EOF
grep -q '";?\[\[:space:\]\]\*' gfd.cpp || { echo "regex patch failed"; exit 1; }
INC="-I. -I$D/src -I$D/src/common -I$D/src/libgfd -I$D/libraries/fmt/include -I$D/libraries/spdlog/include \
-I$D/libraries/excmd/src -I$D/libraries/cpp-peglib -I$D/libraries/gsl-lite/include -I$D/libraries/cnl/include"
FLAGS="-std=c++17 -O1 -w -DFMT_HEADER_ONLY -DSPDLOG_FMT_EXTERNAL -DSPDLOG_HEADER_ONLY"
for f in *.cpp "$D"/src/libgfd/src/*.cpp "$D"/src/common/src/assert.cpp "$D"/src/common/src/log.cpp \
         "$D"/src/libgpu/latte/latte_enum_as_string.cpp; do
  g++ $FLAGS $INC -c "$f" -o "$(basename "$f").o"
done
for f in "$D"/src/common/src/platform_posix_*.cpp; do
  g++ $FLAGS $INC -c "$f" -o "$(basename "$f").o" 2>/dev/null || echo "skipped $(basename "$f") (not needed)"
done
g++ -o latte-assembler ./*.o -lpthread
echo "built $B/latte-assembler"
