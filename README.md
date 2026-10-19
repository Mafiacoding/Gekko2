# Gekko2

Experimental PlayStation 2 emulator for the **original Nintendo Wii**.
A custom PowerPC JIT, software GS reference renderer and experimental GX hybrid renderer.
Independent homebrew project, previously called PCSX2-Wii.

**Technical alpha preparation — games and full-speed operation are not verified.**
BIOS startup has been observed on Wii. Missing BIOS tiles, game compatibility,
performance and hardware accuracy remain active work. No playable PS2 alpha is promised.

## Current development

- **Main source checkpoint:** R1389-SPEED-BETA12.20-GX-ALPHA-FENCES.
- **Current development candidate:** [consolidated draft PR #51](https://github.com/Mafiacoding/Gekko2/pull/51).
  This includes later work and is awaiting review and Wii acceptance.
- [Candidate user guide](https://github.com/Mafiacoding/Gekko2/blob/codex/alpha-frontend-github-cleanup/docs/USER-GUIDE.md)
  — SD installation, controls and optional logs.
- [Candidate build guide](https://github.com/Mafiacoding/Gekko2/blob/codex/alpha-frontend-github-cleanup/docs/BUILDING.md)
  — exact build script, verification and flags.
- [Candidate documentation index](https://github.com/Mafiacoding/Gekko2/blob/codex/alpha-frontend-github-cleanup/docs/README.md)
  and [alpha blockers](https://github.com/Mafiacoding/Gekko2/blob/codex/alpha-frontend-github-cleanup/docs/ALPHA-READINESS-2026-10-19.md).

The candidate adds a loading screen by default and **Settings → On-screen logs**
for optional diagnostic display. These changes are in the draft branch, not yet
in main. SD diagnostics and the separate, optional network logger remain available.
Historical BETA downloads are test checkpoints and fallback builds, not a public-alpha guarantee.

## Toolchain

Use the **latest official devkitPro/devkitPPC and libogc**, including libfat,
wiiuse and bte, from [devkitPro](https://devkitpro.org/wiki/Getting_Started).
Old bundled SDK archives and archived r32/libogc 1.8 instructions are not the
supported setup. The candidate has been built with **devkitPPC r50 / GCC 16.1.0 /
libogc 3.1.0**. These are recorded tested versions, not a claim that every newer
SDK or the historical main checkpoint has been retested. Follow the guide for
the branch you actually build; host tests do not establish physical Wii FPS.

## Installation and privacy

A Wii package places `boot.dol`, `meta.xml` and `icon.png` in `sd:/apps/Gekko2/`.
Supply your own BIOS in `sd:/pcsx2/bios/` and your own disc images separately.
No BIOS, games or SDK binaries are distributed. Back up the working DOL,
settings and memory cards before replacing a test build. Do not attach BIOS,
disc images, guest RAM or private memory cards to public issues.

## History

The previous main README, including historical implementation notes and gallery,
is preserved in [the archive](docs/archive/README-MAIN-2026-10-19.md).
For checkpoint-specific technical evidence use [STATUS.md](STATUS.md) and the
versioned reports; do not combine different builds into a claim of game support.

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
