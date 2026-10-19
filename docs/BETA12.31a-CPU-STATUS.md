# BETA12.31a CPU status correction

A software-renderer comparison can also use the interpreter CPU profile. The
launcher previously displayed the compiled-in `PPC JIT` label even when every
JIT was disabled by the active configuration. That label did not prove native
execution and made renderer comparisons misleading.

The launcher now displays `CPU ACTIVE` for a paused session and `CPU NEXT BOOT`
before a session exists. `JIT ENABLED`, `MIXED CPU` and `INTERPRETER` reflect the
corresponding mask, not the build capability. Enabled JIT still falls back for
unsupported instructions. Changes requested while paused do not relabel the
active session. CPU-profile and renderer help explain their independence;
no saved setting is silently enabled or reset.

The runtime-profile analyzer reports only whitelisted active mask/JIT booleans
and warns about interpreter or mixed-configuration runs. Missing masks and zero
native-attempt work do not establish that JIT is disabled. Timing percentages
retain separate sampled-CPU and exact-presentation denominators. Owner logs,
BIOS data and derived traces remain outside Git and packages.

## Verification

Run `python3 tools/test_beta12_31a_status.py`: four targeted suites cover actual
menu text, requested/active state across cold boot, mixed engines, persistence,
version validation, private-data exclusion and configuration warnings. The old
R1331 invalid-version fixture now uses version 10: version 8 became valid before
this patch. A first run exposed this stale assertion; it was corrected and rerun.

Build in a fresh directory with devkitPPC r50/GCC 16.1.0/libogc 3.1.0:
`tools/build_beta12_31a_status.sh`. SPEED, DSI, AOT, GX and chain compile flags
match BETA12.31; chain/PSM/risk defaults and saved settings are preserved.
DOL/ELF section equality and the static MEM1 gate are checked by the script.
Actual result: all four targeted suites passed. The fresh chain-capable Wii
build passed DOL/ELF equality and MEM1 checks; static end `0x815dae34`,
2,249,164 bytes remain. No physical Wii/Dolphin run of this patch.
There are no core/JIT/GX changes in this patch. Previous BETA12.31 full-suite
results belong to that earlier commit and are not new tests of this patch.

## Short owner test

Settings: `CPU profile = WII BASE`, `Renderer = SOFTWARE REFERENCE`, experiments
OFF, then `COLD BOOT / NEW LOG`. Run for 30–60 seconds and pause. Expect
`CPU ACTIVE: JIT ENABLED`. The event mask and nonzero native retired work in the
log establish the actual path. No need to wait for a complete BIOS boot for
this configuration check. Retain the prior DOL as rollback.

## Still open

The observed GX fence failure, dark/missing BIOS cubes, game compatibility and
physical-Wii performance improvement are not fixed or verified by this patch.
Do not disable the GX ownership/fence protection or infer an FPS improvement
from the CPU label. Hardware visual diagnosis still needs matched renderer
sessions with the same active CPU mask.
