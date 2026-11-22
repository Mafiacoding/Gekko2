#!/bin/sh
# GPL-3.0-or-later. Host test of the launcher menu: fake SD tree, button edges, PPM dumps of the pages.
set -e
cd "$(dirname "$0")/../.."
out=${1:-$(mktemp -d)}
gcc -O1 -w -Iinclude -Isource -Iwiiu/app wiiu/tools/menu_smoke.c wiiu/app/wiiu_menu.c source/hw/frontend_browser.c source/hw/frontend_settings.c \
    source/core/recompiler/optimization.c source/core/recompiler/experimental_options.c -o "$out/menu_smoke"
(cd "$out" && ./menu_smoke .)
