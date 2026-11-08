# Wii toolchain — devkitPPC r51 / GCC 16.2.0 / libogc 3.2.1

Date: 2026-11-08 · Branch `codex/ps2recomp-bridge-batch-a-memory`

This document records the toolchain Gekko2 was **actually built with in this
update**, how to reproduce it, and what was and was not verified.

## Verified result (this update)

| Component | Package (official devkitPro) | Installed version |
|---|---|---|
| devkitPPC | `devkitPPC` | **r51-1** |
| Compiler | `devkitppc-gcc` | **GCC 16.2.0-1** (`powerpc-eabi-gcc (devkitPPC) 16.2.0`) |
| Binutils | `devkitppc-binutils` | **2.47-2** (`GNU ld 2.47.20260726`) |
| C library | `devkitppc-newlib` | 4.6.0.20260123-4 (provides libc, libm) |
| Startup/linker scripts | `devkitppc-crtls`, `devkitppc-rules` | 2.1.0-1, 1.2.1-1 |
| Wii/GC platform library | `libogc` | **3.2.1-1** (header `_V_MAJOR_.._V_PATCH_` = 3.2.1) |
| FAT filesystem | `libfat-ogc` | **2.1.0-4** |
| DOL tools | `general-tools`, `gamecube-tools`, `wiiload` | 1.4.4-1, 1.0.7-1, 0.5.3-1 |
| Examples | `wii-examples` | 20260510-1 |

All of these match the reference versions of the 2026-11-08 research (r51,
GCC 16.2.0, binutils 2.47, libogc 3.2.1, newlib 4.6.0). Package build dates:
2026-10-10 … 2026-10-15 for the compiler stack and libogc.

### How the toolchain was obtained in this session (and a caveat)

The build container has no devkitPro install, and `pkg.devkitpro.org` /
`devkitpro.org` answer HTTP 403 (Cloudflare challenge) to this sandbox, so
**`pacman -Syu` could not be run and "newest package in the repository" is not
independently confirmed**. The toolchain was instead taken from the official
Docker Hub image `devkitpro/devkitppc` (maintainer: devkitPro developers),
manifest-list digest
`sha256:04bab308c032848c1ca5a09c02b4aaa4d505efd7996aab05b9a4eb15231b2868`,
whose layers were fetched over the registry API with each blob's SHA-256
verified and extracted into a scratch directory. The installed package list is
read from the image's own pacman database
(`$DEVKITPRO/pacman/var/lib/pacman/local`). No unofficial compiler builds were
used. A native `pacman -Syu` on a machine that can reach the repository should
report the same set or newer; if it is newer, rebuild and update this file.

### Builds and checks performed (host: Linux x86-64; none of this is a Wii run)

| Step | Result |
|---|---|
| Clean default build, `make BUILD=… TARGET=…` | exit 0; 67 warnings, 0 errors; `Gekko2-…dol` 1,637,216 bytes |
| Clean AOT/DSI option build, `tools/build_r1389.sh` | exit 0; 126 warnings, 0 errors |
| ELF check | ELF32, big endian, PowerPC, EXEC, entry `0x80003f00`, hard-float EABI (`Tag_GNU_Power_ABI_FP: hard float`) |
| `tools/verify_wii_memory.py` | passed: MEM1 static end `0x814d43e0` (default) / `0x815296e0` (options), 3.32 MB / 2.98 MB free (≥ 1 MiB required) |
| DOL structure | entry `0x80003f00`; text/data sections inside the file; the repo's fallback `tools/elf2dol.c` output is byte-identical to devkitPro `elf2dol` |
| Reproducibility | the default DOL has SHA-256 `64a51f09…262d` in two separate clean builds with different build directories and target names |
| `tools/verify_wii_system_r1330i.py` / `verify_wii_perf_r1330i.py` | both PASS (9 linked EE/IOP/SIF grants; 42 paired PPC grants) — executed under the **Unicorn** PowerPC model (`unicorn` 2.1.4), i.e. modelled hardware |
| Host tests | see "Host tests" below |

**Not verified:** boot, rendering, input, audio or speed on a physical Wii or
Dolphin. A successful cross-build and a Unicorn-modelled check are not evidence
of runtime behaviour. No FPS or compatibility claim is made.

### Warnings

- 55 × `-Wmisleading-indentation`, 7 × `-Wunused-function` (libchdr/flac),
  2 × `-Wunused-variable`, 1 × `-Wparentheses`, 1 × `-Wunused-but-set-variable`
  in project/third-party code. These are source style findings, unchanged by
  this update (no source was edited to silence them).
- One linker warning from the toolchain's own `ecrtn.o`: *missing
  `.note.GNU-stack` section implies executable stack* (binutils 2.47; marked
  deprecated). The Wii has no MMU-enforced NX stack, the object is part of the
  devkitPPC runtime and not Gekko2 code; it is expected to be fixed upstream.
- No deprecated-libogc-API diagnostics were produced.

## Libraries actually required (from the repository's includes and link line)

Link line (unchanged): `-lfat -lwiiuse -lbte -logc -lm` with
`-mrvl -mcpu=750 -meabi -mhard-float -DGEKKO` from `wii_rules`.

| Library | Provided by | Why Gekko2 needs it |
|---|---|---|
| libogc (`-logc`) | `libogc` | GX renderer (`gccore.h`), VIDEO, PAD, SYS, cache (`ogc/cache.h`, `DCFlushRange`/`ICInvalidateRange`), `ogc/ipc.h`, `ogc/es.h` and IOS reload/IOS open (BIOS, mload, ARM IOS service), `ogc/audio.h` (AI DMA audio), `ogc/usb.h`/`ogc/usbstorage.h`, `ogc/lwp_watchdog.h`, `ogc/machine/processor.h` |
| libwiiuse (`-lwiiuse`) | `libogc` | `wiiuse/wpad.h` — Wii Remote and Nunchuk input |
| libbte (`-lbte`) | `libogc` | Bluetooth stack required by wiiuse |
| libfat (`-lfat`) | `libfat-ogc` | `fat.h` (`fatInit`), `sdcard/wiisd_io.h`: SD/SDHC and USB FAT access, legacy `sd:/pcsx2/` paths |
| libm (`-lm`), libc | `devkitppc-newlib` | math and stdio |
| libgcc | `devkitppc-gcc` | compiler runtime |

Linker order `-lfat -lwiiuse -lbte -logc -lm` is correct for static libs:
dependents (fat, wiiuse, bte) precede libogc, libm last. Bundled in-tree (no
portlib): libchdr with miniz, LZMA, zstd and FLAC support, lightrec/lightning,
`third_party/wiidrc`.

**Not needed, therefore not installed:** zlib, libpng, libjpeg-turbo, freetype,
libvorbis/libogg, mpg123, SDL2 and other portlibs (no include or symbol in the
tree; compression is in-tree), OpenGX (the renderer calls GX directly),
`aesnd`/`asnd` (`AESND` appears only in a comment; audio uses `ogc/audio.h`),
network/BBA (no `net_*`/`network.h` use), ISFS/DI/EXI APIs (no use; IOS access
goes through `ogc/ipc.h`/`ogc/es.h`). The research list contains `wii-tools`;
there is no package of that name — the DOL tools come with `general-tools` and
`gamecube-tools`.

## Project audit summary

- **Makefile**: includes `$(DEVKITPPC)/wii_rules`, which still exists in r51;
  no change needed. `CFLAGS` use `-mcpu=750 -meabi -mhard-float -mrvl` (Broadway
  superset), correct PowerPC EABI hard-float ABI as confirmed by the ELF
  attributes. `INCLUDE`/`LIBPATHS` come from `PORTLIBS`/`LIBOGC_LIB`.
- **DOL**: `elf2dol` is taken from `$DEVKITPRO/tools/bin` when on `PATH`, else
  `tools/elf2dol.c`/`.py`; both produce identical output here.
- **MEM1/MEM2**: static data must stay in MEM1 with ≥ 1 MiB headroom —
  enforced by `tools/verify_wii_memory.py`. Large buffers (BIOS image, GX
  FIFO/texture/readback, ARM data) use `memalign(32, …)`; no custom stack/heap
  linker script exists (default devkitPPC `ogc.ld`).
- **Cache coherence**: `DCFlushRange`/`ICInvalidateRange` are used in 8+
  places (dynarec/AOT, GX, IPC buffers). Unchanged; not re-validated on hardware.
- **CI**: previously only `ps2recomp-bridge-host-test.yml` existed (host). A
  new `.github/workflows/wii-build.yml` builds both configurations in the
  digest-pinned official image and runs the Unicorn verifiers. It has **not
  yet run on GitHub**; its commands were executed locally with the same tools.
- No source changes were required for r51/libogc 3.2.1.

## Reproduce

### Any OS with Docker (verified approach: the same image)

```sh
docker run --rm -v "$PWD":/work -w /work \
  devkitpro/devkitppc@sha256:04bab308c032848c1ca5a09c02b4aaa4d505efd7996aab05b9a4eb15231b2868 \
  bash tools/build_wii_toolchain.sh
```

### Windows (official installer; steps from the devkitPro documentation, not executed in this sandbox)

1. Install the devkitPro Updater/installer from
   <https://github.com/devkitPro/installer/releases>; choose **Wii** (installs
   to `C:\devkitPro` with its own MSYS2).
2. Open *MSYS2 MSYS* from the devkitPro start-menu folder and run:
   ```sh
   pacman -Syu          # repeat if it asks to restart
   pacman -S --needed wii-dev
   pacman -Q devkitPPC devkitppc-gcc devkitppc-binutils libogc libfat-ogc
   ```
   The `wii-dev` group contains devkitPPC, `devkitppc-gcc/binutils/crtls/rules`,
   `libogc`, `libfat-ogc`, `gamecube-tools`, `general-tools`, `wiiload`,
   `wii-examples`, `wii-cmake`, `wii-pkg-config` and `libgxflux` (verified from
   the package database). No extra portlibs are required.
3. Environment (set by the installer inside MSYS2; set them yourself in other
   shells): `DEVKITPRO=/opt/devkitpro`, `DEVKITPPC=/opt/devkitpro/devkitPPC`,
   and `PATH` including `$DEVKITPRO/tools/bin` and `$DEVKITPPC/bin`.
   (Native Windows paths: `C:\devkitPro`.)
4. Build from the repository root in MSYS2:
   ```sh
   GEKKO2_EXPECT_GCC=16.2.0 GEKKO2_EXPECT_LIBOGC=3.2.1 bash tools/build_wii_toolchain.sh
   # or just: make -j4            # default configuration -> Gekko2.elf / Gekko2.dol
   ```

### Linux without Docker

Install devkitPro's `dkp-pacman` from the official apt/pacman packages (see the
devkitPro "Getting Started" page), then `sudo dkp-pacman -S --needed wii-dev`.

Python 3 is needed for `tools/verify_wii_memory.py` and the AOT generators;
`unicorn` (`pip install unicorn`) only for the linked-PPC verifiers.

## Known compatibility notes

- `tools/build_r13xx.sh` scripts hard-code historical build/target names and
  some mention "recovered devkitPPC r50"; they work unchanged with r51.
  `README.md`/`STATUS.md` historical entries still describe r50/libogc 3.1.0
  builds and are left as history.
- Binary differences versus r50 are expected (new GCC); the DOL hash differs
  from any earlier build.
- The executable-stack linker warning above is a toolchain runtime issue.
- Do not set `DEVKITPRO` to a Windows path inside MSYS2.

## Host tests (run in this update, Linux x86-64, host gcc)

All 333 `tests/test_*.c` programs were built and run (cached host core via
`tools/build_host_core.py`, failures re-run with the official
`tests/run_test.sh` using the CI flags) plus the 5 Python tests. Host tests use
the host compiler and do not depend on the Wii toolchain; no source file was
changed by this update.

- **Pass: 295 of 338** (including `test_ps2recomp_strict_parser` 48 tests,
  `test_ps2recomp_a11_aot_diff` 6 differential runs, `test_ee_aot_inputs`,
  `test_native_hybrid_compile`, `test_beta12_19_dispatch_guard`, and the CI set
  `test_ps2recomp_bridge`, `test_ps2recomp_bridge_window`,
  `test_ee_residency_generation`, `test_ee_exec_lifecycle`).
- **Not passing: 43**, none attributable to the toolchain. Causes seen:
  - private/generated fixtures absent from the public tree (`aot_test_fixture.h`,
    `vu_aot_test_fixture.h`, `iop_aot_test_fixture.h`; `test_ee_aot`,
    `test_vu_aot`, `test_iop_aot`; `test_ps2recomp_a11_aot_diff.c` is the C half
    driven by its Python runner, which passes);
  - tests that need Wii headers (`gccore.h`: `test_wii_arm_ios_beta`,
    `test_controller_usb_r1364`) or the libchdr sources in the link line
    (`chd_get_metadata`/`chd_read`/`chd_close` undefined in several tests);
  - tests that need arguments/feature macros (`test_arm_loader_r1337` needs
    `argc==2`; `-DGEKKO2_IDLE_SKIP` tests exit 77/skip);
  - failing assertions in older tests (e.g. `test_gx_flat_r1290/1294`,
    `test_dma_chain`, `test_optimization_r1331`, `test_lightrec_iop_r1358`,
    `test_gs_*`) that already depend on code unrelated to the compiler.
  They were not baseline-compared against an older commit; they are listed so
  they are not mistaken for passes. Per-test logs are not committed.

## ARM IOS worker

`arm/` (worker.c, ios_service.c, start.S, ios_syscalls.S, worker.ld) has no
build recipe in the repository. With the bundled devkitARM
(`arm-none-eabi-gcc` 16.1.0) the four sources compile
(`-mcpu=arm926ej-s -marm -ffreestanding -nostdlib -O2`); linking with
`worker.ld` was not attempted and no ARM ELF was produced or tested. ARM
handshake/mload behaviour needs a Wii with the matching ELF installed.
