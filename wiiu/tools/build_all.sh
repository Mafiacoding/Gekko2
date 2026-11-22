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

step "GX2 probe"
if make -C wiiu/gx2/probe -j4; then cp wiiu/gx2/probe/gekko2-wiiu-gx2probe.rpx "$OUT/"; else echo "FAILED: gx2probe"; fail=1; fi

step "shaders (latte-assembler)"
if command -v latte-assembler >/dev/null 2>&1; then
  latte-assembler --help 2>&1 | head -20
  # Form used by the wut samples; if your version differs, the help text above shows the right one.
  if latte-assembler compile "$OUT/present.gsh" wiiu/gx2/shaders/present.vsh wiiu/gx2/shaders/present.psh; then
    echo "present.gsh written"
  else
    echo "latte-assembler FAILED: send the exact error text above"
    fail=1
  fi
else
  skipped_shaders=1
  echo "latte-assembler not found. Try: pacman -Ss latte   (or pacman -S wiiu-dev), then run this again."
fi

step "gekko2-wiiu (interpreter + menu)"
if make -C wiiu -j4; then cp wiiu/gekko2-wiiu.rpx "$OUT/"; else echo "FAILED: gekko2-wiiu"; fail=1; fi

step "gekko2-wiiu-jit (EE dynarec, experimental)"
if make -C wiiu JIT=1 -j4; then cp wiiu/gekko2-wiiu-jit.rpx "$OUT/"; else echo "FAILED: gekko2-wiiu-jit"; fail=1; fi

step "results in $OUT"
( cd "$OUT" && ls -l && sha256sum * 2>/dev/null | cut -c1-16,65- )
[ "$skipped_shaders" = 1 ] && echo "NOTE: present.gsh was NOT built (latte-assembler missing); GX2 test A needs it."
[ "$fail" = 0 ] && echo "ALL BUILT STEPS OK" || echo "SOME STEPS FAILED (see above)"
exit "$fail"
