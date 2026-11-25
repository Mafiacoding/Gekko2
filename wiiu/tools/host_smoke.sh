#!/usr/bin/env bash
# GPL-3.0-or-later. Native host build + run of the portable Wii U run loop (synthetic BIOS only).
set -euo pipefail
cd "$(dirname "$0")/../.."
out=${1:-$(mktemp -d)}
CF="-O1 -w -DGEKKO2_WIIU -Iinclude -Isource -Iwiiu/app -Ithird_party/libchdr/include -Ithird_party/libchdr/deps/lzma-25.01/include -Ithird_party/libchdr/deps/miniz-3.1.1 -Ithird_party/libchdr/deps/zstd-1.5.7 -DWANT_RAW_DATA_SECTOR=1 -DWANT_SUBCODE=1 -DVERIFY_BLOCK_CRC=1 -DMINIZ_NO_ARCHIVE_APIS -DMINIZ_NO_DEFLATE_APIS -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE=1"
{ find source -name '*.c' ! -name main.c ! -name ppc_dynarec.c
  ls third_party/libchdr/src/*.c third_party/libchdr/deps/lzma-25.01/src/*.c third_party/libchdr/deps/miniz-3.1.1/*.c third_party/libchdr/deps/zstd-1.5.7/*.c
  ls wiiu/app/g2_app.c wiiu/app/host_main.c; } > "$out/files.txt"
xargs -P"$(nproc)" -I{} sh -c 'gcc '"$CF"' -c {} -o "'"$out"'/$(echo {} | tr / _).o"' < "$out/files.txt"
gcc "$out"/*.o -lm -o "$out/g2_host_smoke"
(cd "$out" && ./g2_host_smoke)
