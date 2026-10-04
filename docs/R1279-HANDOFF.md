# R1279 — Wii Remote and optional Nunchuk

This fixes the launcher using only PAD_Init/PAD_ScanPads (GameCube pads).
Both Interpreter and JIT builds now initialize WPAD/Bluetooth, request button
and extension data, poll WPAD in the launcher, browser, boot and error prompts,
and link the provided libwiiuse and libbte. One DOL supports a Remote alone,
a Remote with Nunchuk, and existing GameCube pads. Controller channel 0 is used.

## Controls (upright Remote)

| Input | Launcher / guest PS2 |
|---|---|
| D-pad | Navigate / PS2 D-pad |
| A / B | Confirm / back; Cross / Circle |
| 1 / 2 | Square / Triangle; 1 toggles update budget in settings |
| + / - | Start / Select |
| HOME | Pause to launcher; return from browser/settings/prompts |
| Nunchuk stick | Digital D-pad navigation / guest D-pad |
| Nunchuk C / Z | L1 / R1; browser page up / down |
| - and + together | Toggle diagnostic HUD |

Nunchuk stick uses calibrated per-axis 35% dead zones. Invalid calibration
and disconnected Remote data produce neutral extension input. Extension bits
are gated by the expansion type. Nunchuk can be attached or removed at runtime;
no separate version is needed. Stick is mapped to digital directions, not a
new implementation of PS2 analog-pad protocol. No sensor bar is needed.
WPAD idle timeout is disabled for long BIOS waits. Existing GameCube input and
its B+Z pause combination remain available.

## Installation
Replace sd:/apps/pcsx2-wii/boot.dol with the R1279 JIT DOL renamed boot.dol.
Keep BIOS, games and existing bios-config.bin in their prior SD locations.
After launch press A if the already paired Remote needs to reconnect.
Boot log is sd:/pcsx2/R1279-boot.log.

## Validation and limits
Both final PPC ELF builds execute mapping tests on a big-endian PPC model:
10 Remote button mappings, HOME excluded from guest input, missing/foreign
expansion ignored, C/Z, all nine stick directions, 768 calibrated axis cases,
and invalid calibration neutral. The inherited actual-ELF EE/IOP/VU/GS/font/
memory regression chain is run against both new binaries.

Physical Bluetooth pairing/reconnection, Nunchuk hotplug and real Wii FPS
cannot be tested in this workspace. Emulator chunks can still make HOME or
guest input respond slowly during BIOS boot; this change does not claim a
speed increase. No new cold boot or Tekken run is claimed. The R1278 native
182-test result and memory measurements are inherited unchanged: core source
is unchanged by R1279. No BIOS/game/raw guest RAM is included in the public ZIP.

Build: tools/build_r1279.sh (separate -j1 Interpreter and JIT directories).
Test: tools/verify_ppc_remote_r1279.py, plus verify_ppc_word_reads_r1278.py
against each R1279 ELF. All cumulative changes remain in Claude patch/diff.
