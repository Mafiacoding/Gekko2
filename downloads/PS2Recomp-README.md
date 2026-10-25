# PS2Recomp Wii owner test candidate

Latest: **P16**, source `eadc4fd`. [Protected owner update](PS2Recomp-Crash-Wii-P16-Update-PASSWORD.zip) · [Source and P15-to-P16 patch](PS2Recomp-Wii-P16-Source.zip) · [Checksums](PS2Recomp-SHA256SUMS.txt).

## P17: alignment diagnostic only

The owner still observes a physical Wii alignment panic with P16. The faulting
instruction shown by the exact ELF is ADDI, and the linked Read128 helper passes
49 modeled PPC cases; this does not explain the hardware failure.
[P17 protected diagnostic](PS2Recomp-Crash-Wii-P17-Diagnostic-PASSWORD.zip) ·
[P17 source and P16-to-P17 patch](PS2Recomp-Wii-P17-Source.zip).

Replace only `sd:/ps2recomp/crash-runtime.dol`; keep the existing HBC loader and
the P16 rollback. One panic photo containing BUILD, PC/LR, DAR/DSISR,
CODE_VALID, CODE_CACHED and CODE_RAM is sufficient for the next distinction.
No additional long BIOS/log collection is requested. The code words are sampled
from cached/uncached RAM before video changes; this is not an I-cache dump.
Six compiled PPC panic tests and the artifact/stack/memory checks pass.
**No alignment-crash fix or physical validation is claimed for P17.**


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
