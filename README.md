# Gekko2

Experimental PlayStation 2 emulator for the **original Nintendo Wii**.
Custom PowerPC JIT, software GS reference renderer and experimental GX hybrid renderer.
Previously known as PCSX2-Wii; an independent homebrew project.

**Development / technical alpha preparation.** BIOS startup has been observed on real Wii.
Games, correct BIOS tiles and full-speed operation are **not verified**. This is not a
complete PS2 emulator. The authoritative build identifier is in
[build_version.h](include/build_version.h); historical SD test packages are not a release guarantee.

| Start here | |
| --- | --- |
| Run on Wii, controls, optional logs | [User guide](docs/USER-GUIDE.md) |
| Compile with current devkitPro / libogc | [Build guide](docs/BUILDING.md) |
| Alpha blockers and actual evidence | [Alpha readiness](docs/ALPHA-READINESS-2026-10-19.md) |
| Technical documentation | [Documentation index](docs/README.md) |
| Detailed development status | [STATUS.md](STATUS.md) |

## Building and testing

Use the **latest official devkitPro/devkitPPC and libogc**, including libfat,
wiiuse and bte. Old bundled SDK archives are not the supported build setup.
The recorded validation environment is **devkitPPC r50 (GCC 16.1.0), libogc 3.1.0**;
this identifies the tested versions, not a guarantee that future SDK changes are compatible.
See the [build guide](docs/BUILDING.md) for installation, flags and verification.

```sh
# DEVKITPRO and DEVKITPPC must point to your official SDK installation.
CHAIN_RESIDENCY=0 bash tools/build_beta12_33_windows.sh
python3 tools/test_alpha_compat_host.py --output-json /tmp/gekko2-host.json
```

Build from the branch you intend to test and keep its commit ID. Development PRs
may contain work that is not merged into `main`. Host and PPC simulation results
are distinct from Dolphin execution and physical Wii tests. No measured Wii FPS
improvement or playable title is promised by these checks.

## Installation

Put the selected build in `sd:/apps/Gekko2/boot.dol`, alongside `meta.xml`
and `icon.png`. Supply your own BIOS as `sd:/pcsx2/bios/bios.bin`
(or `SCPH50004.bin`); no BIOS or games are included.
See the [user guide](docs/USER-GUIDE.md) before replacing an existing installation.
The loading screen is the default. **Settings → On-screen logs** enables the
saved diagnostic display; SD logs and the optional network logger are separate.

## Project history

Earlier implementation notes, captures and release descriptions are preserved in
[the archived README](docs/archive/README-HISTORY.md). Use current documentation
for supported tooling and limitations.

## Credits and license

Thank you to the **PCSX2 team and contributors** for their PS2 emulator, code, documentation and research. EE instruction semantics originated from PCSX2 references/ports; attribution and GPL notices remain in the source. The name change does not remove those origins.

- [PCSX2](https://pcsx2.net/) / [source](https://github.com/PCSX2/pcsx2)
- [ps2sdk](https://github.com/ps2dev/ps2sdk): PS2 interfaces and reference documentation.
- [devkitPro](https://devkitpro.org/) and [libogc](https://github.com/devkitPro/libogc): Wii toolchain and platform libraries.
- Bundled ELF-to-DOL/wiiload tools retain their individual licenses. Font attribution is in [docs/licenses/DejaVu.txt](docs/licenses/DejaVu.txt).
- Alex: project development, hardware testing and direction, with Claude AI and ChatGPT/GPT-6.1 assistance.
- Gekko2 logo artwork was created with image-generation assistance; see [branding notes](docs/BRANDING-R1302.md).

The project is **GPL-3.0**; see [COPYING.GPLv3](COPYING.GPLv3). Third-party files retain their own notices and terms. Gekko2 is independent of the credited teams.

BIOS ROMs, game images, private guest RAM/checkpoints, personal runtime logs, BIOS configuration and SDK/compiler binaries are excluded from the public source tree.
