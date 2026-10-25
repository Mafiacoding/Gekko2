# PS2Recomp Wii owner test candidate

Latest: **P16**, source `eadc4fd`. [Protected owner update](PS2Recomp-Crash-Wii-P16-Update-PASSWORD.zip) · [Source and P15-to-P16 patch](PS2Recomp-Wii-P16-Source.zip) · [Checksums](PS2Recomp-SHA256SUMS.txt).

The AES-protected update is for the owner of the supplied game. No disc or BIOS
is supplied. Keep the existing owner game files. The HBC loader stays unchanged;
the replacement runtime is `sd:/ps2recomp/crash-runtime.dol`. Do not rename the
large runtime to the HBC loader's `boot.dol`. The password is shared privately.
P15 remains available as a rollback.

Correct R5900 SQRT.S operand selection, VU clipping/timing and LIBGS buffer ABI.
Local cold runs now reach title, loading and the player/corridor/HUD scene;
a Cross input produces a visible jump. These are host observations. The Wii
cross-build passes DOL/ELF/static-memory/stack checks, plus simulated PPC tests.
There is no physical Wii or Dolphin validation or verified Wii FPS gain.
The previous photographed alignment failure has not been independently resolved
on hardware. Other graphics, audio and game compatibility remain incomplete.

Build with current official devkitPro/devkitPPC and libogc; this candidate uses
devkitPPC r50 / GCC 16.1.0 / libogc 3.1.0. Full source and test commands accompany
the source archive. Preserve license notices and keep generated owner game code
private.

The separate [guarded native-hybrid compiler draft](https://github.com/Mafiacoding/Gekko2/pull/56)
reuses Gekko2's AOT/interpreter state and has 512 synthetic handoff comparisons.
It is not yet the complete shared PS2Recomp/Gekko2 GS/GX runtime or an automatic
ISO-to-playable-DOL converter. See its report for the open integration work.
