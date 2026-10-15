# R1337 Beta ARM

This beta adds startup IOS222 selection when the saved ARM worker option is ON.
The current IOS remains selected when ARM is OFF. Change ARM, save options and
restart the emulator to apply IOS selection; cold boot alone cannot reload IOS.

Requires an installed compatible Hermes cIOS222 with base IOS38. This app does
not install/patch IOS. Missing/stub IOS222 is rejected before any reload. An
IOS reload or MLOAD probe failure restores the original IOS when possible;
storage/controller initialization runs again. A failed recovery aborts startup.

Copy apps/Gekko2/ to SD apps/. Copy arm/Gekko2-ARM-Worker.elf to
sd:/pcsx2/arm/Gekko2-ARM-Worker.elf. Enable ARM, exit to HBC and restart this beta.
Use cold boot/new log. Keep your existing BIOS/game/options files.

ARM_IOS status=1 active=222 means IOS/MLOAD selection succeeded.
ARM_LOADER status=1 means worker thread started.
ARM_WORKER available=1 means capability handshake succeeded.
Only submitted/completed count actual CSC jobs. BIOS startup may issue none.

ARM_IOS: 0 unchanged, -1 missing/stub/invalid IOS222, -2 reload failure,
-3 no MLOAD, -4 original IOS recovery failed, -5 storage mount failure.

Eight native ASan/UBSan mocked lifecycle scenarios pass, including startup,
stub rejection, failed reload, MLOAD absence, rollback and SD remount failure.
Actual linked PPC guard/ARM ownership-wakeup/MPEG tests and DOL/ELF byte checks
pass. SDK/IOS transport is mocked. Physical Wii IOS reload/ARM loading and
performance remain unverified. Earlier R1337's 33 full PPC checks are baseline
evidence; this beta ran the focused checks listed above, not a new full 33 run.

Build: sh tools/crossbuild_r1337_beta.sh with devkitPPC/libogc.
All original R1337 decoder/cache fixes remain. No BIOS/game/SDK is included.
