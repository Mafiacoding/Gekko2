# Build and verification

Install and update the **latest official devkitPro/devkitPPC and libogc** using
[devkitPro's installation instructions](https://devkitpro.org/wiki/Getting_Started)
and its package manager. Install/update the Wii development packages (including
libogc, libfat, wiiuse and bte); do not use the historical r32 / libogc 1.8 SDK
mentioned in archived reports. On Windows use the supported devkitPro/MSYS2 shell.

The recorded build environment on 19 October 2026 is devkitPPC r50,
GCC 16.1.0, libogc 3.1.0. Those are **tested versions**. If a newer SDK changes
an API, include the exact SDK versions and error when reporting a build failure.
Toolchains and libraries are not bundled with this source.

```sh
export DEVKITPRO=/opt/devkitpro # use your installation path
export DEVKITPPC="$DEVKITPRO/devkitPPC"
export PATH="$DEVKITPPC/bin:$DEVKITPRO/tools/bin:$PATH"
CHAIN_RESIDENCY=0 bash tools/build_beta12_33_windows.sh
```

The script preserves the existing SPEED/AOT/DSI/GX-fence flags, gives each commit
its own object directory, and checks DOL/ELF identity plus static MEM1 headroom.
Output: `Gekko2-BETA12.33-GX.elf` and `.dol`. The control build omits experimental
128-bit chain compilation. For a separate comparison, `CHAIN_RESIDENCY=1` creates
the `Gekko2-BETA12.33-GX-Chain` pair; runtime residency still defaults OFF.
Never reuse object directories after changing flags at the same commit: remove
that variant's ignored build directory first, or select a fresh build directory.
The BETA12.33 name is a development identifier, not an alpha release declaration.

## Host regression gates

Install a C/C++ compiler, make and Python 3. Some tools require Python Pillow;
linked PPC verification additionally requires Unicorn and SDK `powerpc-eabi-nm`.

```sh
python3 tools/test_alpha_compat_host.py --output-json /tmp/gekko2-host.json
python3 tools/verify_dol_r1352.py Gekko2-BETA12.33-GX.dol Gekko2-BETA12.33-GX.elf
python3 tools/verify_wii_memory.py Gekko2-BETA12.33-GX.elf
```

The host runner currently covers 90 groups. Individual linked-PPC tools in
`tools/verify_*_linked.py` take an ELF and `--nm "$DEVKITPPC/bin/powerpc-eabi-nm"`.
Record the exact ELF hash and commit alongside results. Static memory checks
cannot prove peak heap use or freedom from runtime GPU faults. Neither host
regressions nor simulated PPC execution establish Wii FPS or game playability.

Review packages can be produced with `tools/package_alpha_review.py` only after
its explicit report gates pass. Do not distribute private BIOS, games, user logs,
RAM dumps, checkpoints, SDK binaries or unreviewed experimental settings.
