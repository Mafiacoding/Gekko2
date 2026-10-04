# R1284 — EE scalar dispatch, GS addressing and experimental GX presentation

Early alpha, BIOS/OSDSYS first. Owner Wii R1279/R1280 confirms Sony splash and Remote reliability. Stable hardware OSDSYS, playable games and any GX speed gain remain unconfirmed.

## Implemented
- EE SLTI/SLTIU, MOVZ/MOVN and HI/LO transfers use inline bodies through the existing exact instruction retirement path; native translator APIs remain. Guest upper halves, zero/aliases, full64-bit comparisons, Count/PC and interrupt/device retirement are preserved.
- PSMCT32 span reads use a 64-entry fixed horizontal address table. All pixels are read from current shared VRAM, with existing bounds and endian conversion.
- Experimental GX presentation uploads current PSMCT32 GS pixels as a tiled RGBA8 texture, draws a bilinear quad, copies EFB to XFB and synchronizes before CPU text. Settings RIGHT toggles it; default OFF. Unsupported format/dimension/AA, allocation or copy-height bounds fall back to software. No guest GS primitive is offloaded yet. Details: GX-PRESENTATION.md.

## Measurements versus R1283
Actual linked PPC instructions under Unicorn; mocked allocation/cache services. Not Wii cycles, FPS or representative BIOS throughput.
- Eight EE retirements: SLTI 4721->4185 (-11.35%), SLTIU4705->4225(-10.20%), MOVZ4777->4249(-11.05%), MOVN4785->4313(-9.86%). HI/LO transfers4657->4233/4257 (-9.10/-8.59%).
- 640 contiguous GS reads:14751->7710 (-47.73%); this is only the row-read helper.
- Complete software scanout 640x16->640x36:1153119->1040463 (-9.77%); identity640x16:722391->609735(-15.60%). Horizontal downscale320x36 remains766767.
- Mixed4096EE/512IOP loop2539780->2542510 (+0.11%). Targeted savings do not establish global CPU/FPS improvement.
- A table-based EE dispatcher increased mixed cost by about2%, and an alternate filter formula made downscaling worse. Neither is retained.
- GX hardware drawing and speed remain unmeasured; CPU texture packing tests alone are not GPU proof.

## Verification
183 native regressions. Both final Interpreter/JIT ELFs pass inherited profile/cache/IRQ/retirement suites, 1024 COP0 and44 timer cases, 512 new EE register-byte oracle cases, 240 IOP controls, 176 EE branches, VU cached-prefix mutation/budget guards, 350 GS spans and scanout filter oracles. Both pass24 independent tiled RGBA8 byte-oracle/guard cases. GX GPU execution is not available in this environment.

The PPC generator itself is unchanged; R1282 generatedVU9760 andEEALU1200 oracle evidence remains applicable to those primitives. The bounded EE ALU compiler is still not integrated with CPU retirement. Complete block execution, linking/register allocation and broader exact exits remain work in progress.

Native continuation from paired R1283 BIOS state advances another roughly3.2M EE instructions without EE/IOP halt (osd_r1284.log). This is warm host-state continuity, not a Wii cold-boot or stable hardware menu proof.

## Installation and hardware comparison
Rename PCSX2-Wii-R1284-Menu-JIT.dol to sd:/apps/pcsx2-wii/boot.dol. Preserve BIOS/config/discs. First test default software; then Settings RIGHT enables GX and resume BIOS. Compare picture, input, FPS and behavior returning HOME to the launcher. RIGHT again disables GX. Supply sd:/pcsx2/R1284-boot.log and pictures if output differs. gx_output records requested mode; individual frames may fall back.

## Distribution and next steps
Public source/patch excludes BIOS, games, guest RAM/checkpoints, personal runtime logs and compiler/SDK. Private saved state is separate. Claude patch/diff are cumulative against the originally uploaded project and byte-verified. GitHub upload patch is cumulative against base68964c8a6ddc08fb4c470e9271cefa5e3d106394, preserving history; upload still pending unavailable write access.

Next: hardware-validate GX presentation; profile true BIOS EE/IOP shares; add limited GS primitive offload only with correct shared VRAM dirty-region resolve/readback, CLUT, blending/depth and framebuffer feedback. Keep software fallback and BIOS correctness throughout.
