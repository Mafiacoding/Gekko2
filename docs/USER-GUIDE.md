# Wii user guide

Gekko2 is an experimental technical test build, not verified playable PS2 software.
Keep a backup of your working DOL, SD settings and memory card before updating.

## SD layout

```text
sd:/apps/Gekko2/boot.dol
sd:/apps/Gekko2/meta.xml
sd:/apps/Gekko2/icon.png
sd:/pcsx2/bios/bios.bin
sd:/pcsx2/games/your-image.iso
```

Use the built DOL, repository `meta.xml` and `assets/branding/icon.png`.
BIOS search order: `SCPH50004.bin`, `SCPH39001.bin`, `SCPH10000.bin`, `bios.bin`.
SCPH-50004 is the primary investigated BIOS; equal compatibility is not established.
Supply only BIOS and disc images you are entitled to use. None are distributed here.
The launcher supports selected ISO/BIN images; it does not expose CHD selection.

## Start and settings

Navigate with D-pad/stick, A to select, B to return. Choose **BIOS / OSDSYS**
for a disc-free session, or **SELECT DISC** then **START DISC**. HOME pauses;
PLUS/START resumes a paused session. **COLD BOOT / NEW LOG** restarts with
requested CPU/GX/experimental settings. Resume keeps the existing active options.
Use **EXIT TO HBC** to leave normally and flush persistent data.

The default diagnostic display is OFF. Before the first real emulated picture,
a loading screen shows the actual stage and elapsed time. The animation is not
a percentage or proof of guest progress; startup can still be extremely slow.
Storage, BIOS, core and disc failures remain visible without diagnostic text.

**Settings → On-screen logs → ON/OFF** saves the display preference to
`sd:/pcsx2/frontend.cfg`. Existing HUD preferences remain compatible; an old ON
setting stays ON. The existing controller diagnostic shortcut can temporarily
toggle it during a session. It does not change the saved preference.
Rate counters have their own switch; output presents are not guest game FPS.

SD session logs in `sd:/pcsx2/logs/` and `sd:/pcsx2/Gekko2-startup.log`
remain available independently of screen text. Network logging remains a
separate, default-OFF option under **Settings → Network logger**.
[Windows receiver instructions](../tools/windows-telemetry/README-DE.md).
Hiding screen text does not disable profiling or speed up emulation by a proven amount.

Do not enable experimental scheduler, PSM, 128-bit residency or GX risk modes
silently for other users. They remain separate opt-in settings and require
controlled comparisons. See [current alpha blockers](ALPHA-READINESS-2026-10-19.md).

## Reporting a problem

Record the build commit, BIOS revision, renderer, CPU profile and active options.
Prefer one bounded SD/network capture of a reproducible session. Never submit BIOS,
disc images, memory cards or guest RAM in a public issue. Redact personal paths
from any shared logs. Current work and test reports are in the [index](README.md).
