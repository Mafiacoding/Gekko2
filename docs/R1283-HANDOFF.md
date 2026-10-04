# R1283 — bounded VU cache validation and IOP dispatch

Early alpha; BIOS/OSDSYS stability comes first. Real Wii Sony splash and reliable Remote are confirmed by the owner on R1279/R1280; stable hardware OSDSYS remains unconfirmed.

## Changes
Warm VU blocks compare only their compiled microinstruction prefix instead of scanning subsequent words. Every upper/lower word remains checked. Cache ownership, micro-mask, data-alias, budget, delayed branch and E-bit guards remain. Changes outside that prefix do not invalidate it; changes inside it require recompilation.

IOP J/JAL, JR/JALR, BEQ/BNE/BLEZ/BGTZ and MFC0/MTC0 effects use the existing inline CPU dispatcher. Standalone translators remain. Delay slots, links, hardware Cause.IP2 refresh and interrupt/device retirement are preserved. Cause.IP2 must be considered when testing MTC0 Cause after retirement.

## Measurements against R1282
Actual linked PPC ELF instructions under Unicorn, with allocation/cache services mocked; not Wii cycles/FPS or a representative BIOS workload:
- Eight-pair warm VI block: 680 -> 558 (-17.94%). ACC: 656 -> 534 (-18.60%). LQ/SQ: 856 -> 734 (-14.25%). I-immediate: 1007 -> 920 (-8.64%).
- Two IOP control retirements: typical J 1955 -> 1918; JAL 1958 -> 1923; taken BEQ 1959 -> 1922. Some scalar paths grow slightly from dispatcher layout changes.
- Mixed 4096 EE / 512 IOP synthetic loop: 2544064 -> 2539780 (-0.17%). Do not extrapolate component benchmarks to global speed.
- Full exact measurements: verification/optimizations_r1283.json in the checkpoint.

## Verification
183 native regressions pass. Both final Interpreter/JIT ELFs pass inherited profile/cache/retirement checks, 1024 COP0 byte-oracle cases and 44 timer cases each. Both pass 240 new IOP branch/COP0 control cases, VU cached-prefix word mutation and budget guards, 350 GS-span cases with scanout filter oracles, and 176 EE short-branch cases.

The PPC code generator is unchanged from R1282, where 9760 generated VU blocks and 1200 EE ALU block oracle cases passed. That ALU primitive is still not enabled in CPU retirement. Full EE/IOP block integration, linking/allocator, exact exits, broader VU coverage and GX rendering remain open.

Native continuation from the paired R1282 BIOS state advances EE 2061015180 -> 2064211186 without EE/IOP halt; this is a warm host-state test, not cold boot or real Wii proof.

## Install and next hardware evidence
Rename PCSX2-Wii-R1283-Menu-JIT.dol to sd:/apps/pcsx2-wii/boot.dol. Preserve BIOS/config/discs. Interpreter build is included for comparison. Provide sd:/pcsx2/R1283-boot.log, especially CPU_SAMPLE and EE_TIMERS. Remote/Nunchuk controls, FPS/HUD options and HBC exit controls remain.

## Distribution and GitHub
Public patch/source excludes BIOS, games, guest RAM/checkpoints, personal runtime logs and compiler/SDK binaries. Private saved state is separate. Claude patch/diff apply to the originally uploaded project; the GitHub upload patch targets repository base 68964c8a6ddc08fb4c470e9271cefa5e3d106394 and includes unpublished R1282 plus R1283 changes. Target: https://github.com/Mafiacoding/PCSX2-Wii. Existing history is preserved.

GitHub is connected, but no callable GitHub write tools are exposed in this session. CLI push previously failed because no HTTPS credentials are available. No successful upload is claimed; prepared commits and a verified public upload patch are retained.
