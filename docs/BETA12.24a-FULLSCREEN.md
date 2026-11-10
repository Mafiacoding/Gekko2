# BETA12.24a – restore full output size

16 October 2026. Base: d819c5c4fae00e69455a4dd5083447fde2bc37ac.
Branch: codex/beta12-24a-fullscreen.
Release: R1389-SPEED-BETA12.24a-GAME-TRACE-FULLSCREEN.

The owner clarified that the oversized/stretched elements were the question-mark
tiles above Tekken's character portraits, rather than the whole game output.
The automatic 4:3 XFB pillarboxing introduced in BETA12.23 was the wrong
response and made the entire image too small. This hotfix removes that call
from presentation, restoring full output size. The unused aspect helper remains
available but is not invoked by the game presentation path.

GAME_TRACE and all other BETA12.24 changes remain. No new texture/CLUT/scaling
fix for the individual question-mark tiles is claimed. The photo shows the
reported layout but cannot establish its GS register/texture cause; a current
Tekken log is needed. No fabricated replacement textures or per-game screen
stretch guesses are introduced. No FPS/gameplay gain is claimed.

Build: tools/build_beta12_24a_fullscreen.sh in the same pinned official
SDK as BETA12.24 (devkitPPC r50/GCC16.1.0/libogc3.1.0). Fresh build directory;
all diagnostic and existing performance flags retained. DOL/ELF and MEM1
checks accompany the package. The 47-suite core results are inherited from
BETA12.24, not rerun for this removal of a presentation postprocess call.
