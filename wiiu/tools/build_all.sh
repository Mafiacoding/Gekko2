#!/usr/bin/env bash
# GPL-3.0-or-later. Builds every Wii U test artifact into wiiu/out/ and prints SHA-256 sums:
#   gekko2-wiiu.rpx (interpreter + launcher menu), gekko2-wiiu-jit.rpx (EE dynarec), gekko2-wiiu-gx2probe.rpx,
#   and present.gsh when latte-assembler is installed. Run from a devkitPro shell (DEVKITPRO and wut set).
set -u
cd "$(dirname "$0")/../.."
: "${DEVKITPRO:=/opt/devkitpro}"
export DEVKITPRO
: "${DEVKITPPC:=$DEVKITPRO/devkitPPC}"
export DEVKITPPC
export PATH="$DEVKITPPC/bin:$DEVKITPRO/tools/bin:$PATH"
OUT=wiiu/out
mkdir -p "$OUT"

# Preflight: the Wii U libraries (wut) are a separate devkitPro package group.
if [ ! -f "$DEVKITPRO/wut/share/wut_rules" ]; then
  echo "The Wii U SDK (wut) is not installed under $DEVKITPRO."
  echo "Contents of $DEVKITPRO:"; ls "$DEVKITPRO" 2>&1 | head -20
  if command -v pacman >/dev/null 2>&1; then
    echo "Installing the devkitPro group wiiu-dev with pacman (needs internet) ..."
    pacman -S --needed --noconfirm wiiu-dev || { echo "pacman could not install wiiu-dev. Run it by hand in the devkitPro shell."; exit 2; }
  else
    echo "pacman not found. In the devkitPro MSYS2 shell run:  pacman -S --needed wiiu-dev"
    exit 2
  fi
  [ -f "$DEVKITPRO/wut/share/wut_rules" ] || { echo "wut is still missing after the install."; exit 2; }
fi
echo "Wii U SDK found: $(ls -d "$DEVKITPRO"/wut 2>/dev/null)"
fail=0
skipped_shaders=0
step() { echo; echo "=== $*"; }

step "shaders (latte-assembler)"
if command -v latte-assembler >/dev/null 2>&1; then
  latte-assembler --help 2>&1 | head -20
  # Form per GaryOderNichts/wiiu-shaders: assemble --vsh=... --psh=... out.gsh ("compile" is the GLSL route and
  # needs the AMD Shader Analyzer, so there is no fallback to it).
  latte-assembler assemble --vsh=wiiu/gx2/shaders/present.vsh --psh=wiiu/gx2/shaders/present.psh "$OUT/present.gsh" 2>&1
  rc=$?
  echo "latte-assembler exit code: $rc"
  if [ -s "$OUT/present.gsh" ]; then
    echo "present.gsh written ($(wc -c < "$OUT/present.gsh") bytes)"
    # Built in (gitignored headers): presenter for the app and the probe, GS renderer shaders for the probe test C.
    python3 wiiu/tools/embed_gsh.py "$OUT/present.gsh" || fail=1
    python3 wiiu/tools/embed_gsh.py "$OUT/present.gsh" --name present_gsh_probe_data --out wiiu/gx2/src/present_gsh_probe.h || fail=1
  else
    echo "present.gsh was NOT written: send the output above"
    fail=1
  fi
  for ps in color:gs_color mod:gs_modulate zload:gs_zload fbcopy:gs_fbcopy; do
    name=${ps%%:*}; file=${ps##*:}
    if latte-assembler assemble --vsh=wiiu/gx2/shaders/gs.vsh --psh=wiiu/gx2/shaders/$file.psh "$OUT/gs_$name.gsh" 2>&1 &&
       [ -s "$OUT/gs_$name.gsh" ]; then
      python3 wiiu/tools/embed_gsh.py "$OUT/gs_$name.gsh" --name gs_${name}_gsh_data --out wiiu/gx2/src/gs_${name}_gsh.h || fail=1
    else
      echo "gs_$name.gsh was NOT written"
      fail=1
    fi
  done
else
  skipped_shaders=1
  echo "latte-assembler not found (devkitPro does not ship it; it is part of decaf-emu): download the Windows artifact from"
  echo "https://github.com/decaf-emu/decaf-emu/actions (latest successful run; needs a GitHub login), put latte-assembler.exe in"
  echo "$DEVKITPRO/tools/bin or on PATH, then run this again."
fi

step "GX2 probe"
if make -C wiiu/gx2/probe -j4; then cp wiiu/gx2/probe/gekko2-wiiu-gx2probe.rpx "$OUT/"; else echo "FAILED: gx2probe"; fail=1; fi

step "gekko2-wiiu (interpreter + menu)"
if make -C wiiu -j4; then cp wiiu/gekko2-wiiu.rpx "$OUT/"; else echo "FAILED: gekko2-wiiu"; fail=1; fi

step "gekko2-wiiu-jit (EE dynarec, experimental)"
if make -C wiiu JIT=1 -j4; then cp wiiu/gekko2-wiiu-jit.rpx "$OUT/"; else echo "FAILED: gekko2-wiiu-jit"; fail=1; fi

step "results in $OUT"
( cd "$OUT" && ls -l && sha256sum * 2>/dev/null | cut -c1-16,65- )
[ "$skipped_shaders" = 1 ] && echo "NOTE: present.gsh was NOT built (latte-assembler missing); GX2 test A needs it."
[ "$fail" = 0 ] && echo "ALL BUILT STEPS OK" || echo "SOME STEPS FAILED (see above)"
exit "$fail"
