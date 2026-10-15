# Gekko2 SPEED BETA12.1 – Crash E3 / LOADFILE continuation

14 October 2026. Based on the user's SPEED BETA12 source archive, not the
older public `main` branch.

## Code changes

1. `sif_loadfile_elf_load_disc`: contiguous physical RAM writes now invalidate
   EE JIT pages once per sector-sized span, not once per byte. Original
   translated-base/delta semantics remain, with the original byte fallback
   for noncontiguous/out-of-range cases. BSS is cleared with the same rule.
2. Reject ELF32 programs with bad format, machine, truncated program header
   table, `p_filesz > p_memsz`, or segment content outside the ISO file.
   Disc read failures no longer become silent success with a partially
   zero-padded program. This is error reporting, not a game-specific HLE hack.
3. Fix the FILEIO `;1` fallback's previous 64-byte array with up to 221
   bytes copied. Keep enough space for the suffix and never look up a
   truncated file name.
4. Fix white-box host test linking of included C translation units. Allow
   tests under `tests/` and nested `#include "test_*.c"` fixtures.
5. Add private-image Crash E3 raw CD / ELF probes, plus a generic extractor for
   text-only ISO9660 `.ATS` scripts and `SYSTEM.CNF`.

## Verification in this continuation

- Native GCC compilation of `ee_core.c`: PASS.
- `test_iso_loader`: PASS.
- `test_iso_nested_path`: PASS.
- `test_iso_cache_r1358`: PASS.
- `test_ee_boot_rpc_errors`: PASS.
- `test_iop_cdvd`: PASS.
- `test_disc_elf_guard_beta12`: PASS. Rejects two malformed ELF cases; loads a
  valid synthetic ELF with file bytes and BSS verified.
- `tools/probe_crash_e3_iso`: PASS on the owner's private 2352-byte Mode 2 BIN.
  `SYSTEM.CNF`, `SCEU_000.01;1` MIPS ELF, ATS and VAG files readable.
- `tools/verify_crash_e3_elf_load`: PASS with the private E3 image: entire
  3,582,440-byte PT_LOAD segment matched the mounted disc, and all 629,540
  trailing BSS bytes were zero. ELF entry point was `0x00100008`.
- Wii DOL cross-build: PASS using devkitPPC 16.1.0/r50 and libogc 3.1.0.
  MEM1 static remaining: 2,706,348 bytes (1 MiB minimum test). Linked ELF
  sections matched DOL; DOL size 2,217,368 bytes.

**Not verified:** a PS2 BIOS or Crash E3 **runtime boot** in Dolphin/Wii,
interactive gameplay, VIF1 stall recovery after Tekken VS, GT3 memory card
support, Wii FPS increase, ARM worker hardware execution. Neither the private
Crash image nor the owner's PS2 BIOS or runtime snapshots are in this source.

## Reproduce the private Crash tests

Use a local copy of your legally obtained BIN, not a public repository file:

```sh
cc -O2 -Iinclude -Isource tools/probe_crash_e3_iso.c -o /tmp/crash_probe
/tmp/crash_probe /path/to/your/Crash-E3.bin

python3 tools/build_host_core.py verify_crash_e3_elf_load test_disc_elf_guard_beta12
GEKKO2_CRASH_E3_BIN=/path/to/your/Crash-E3.bin \
  validation/r1386-host/<flags-hash>/verify_crash_e3_elf_load

python3 tools/extract_iso_text_beta12.py /path/to/your/Crash-E3.bin \
  --out /tmp/Crash-E3-text.txt
```

To cross-build on a configured Wii SDK:

```sh
export DEVKITPRO=/path/to/devkitpro
export DEVKITPPC="$DEVKITPRO/devkitPPC"
export PATH="$DEVKITPPC/bin:$DEVKITPRO/tools/bin:$PATH"
sh tools/build_speed_beta12.sh
```

The release checkpoint is labeled **BETA12.1** to distinguish it from the
prior owner's BETA12. The source remains GPL-3.0 with the original
attributions. Do not infer a new emulator-compatible game from successful
file parsing or ELF relocation.
