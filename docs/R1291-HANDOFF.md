# R1291 — BIOS first-image and GX startup handoff

2026-10-04. Early alpha. Hardware cold boot remains to be verified.

## Evidence and fix

The supplied R1290 Wii log records software mode throughout: FIRST_IMAGE at EE 295401012, 465217 ms (about 7m45s). It does not contain the failed GX run. An overwritten comparison log is possible, not proven.

First-image detection previously ran only while the diagnostic HUD was hidden. The HUD could therefore suppress detection indefinitely. R1291 probes VRAM before the HUD presentation branch. The bounded probe checks at most 512 sample points on due presentations. The waiting message now describes BIOS pixels, and a detected image remains recorded with the HUD visible.

## GX controls and logging

RIGHT toggles GX output only. LEFT independently toggles experimental flat primitive drawing; enabling drawing also enables output. Drawing is disabled by default and cannot activate before the first BIOS image. Keep LEFT off for the first GX output comparison.

GX ready means the output backend has initialized, which occurs on first GX presentation after pixels are detected and the HUD is hidden. Thus first=1, ready=0 can legitimately occur with the HUD visible. Hide the HUD using MINUS+PLUS (Remote) or Z+START (GameCube).

Separate mode logs preserve software versus GX comparisons:
- sd:/pcsx2/R1291-software.log
- sd:/pcsx2/R1291-gx-output.log
- sd:/pcsx2/R1291-gx-render.log

Each fresh run can still replace the previous log of the same mode. Copy it before repeating that mode. Diagnostics include actual HUD state, backend readiness, requested/active primitive mode, output attempts/fallbacks and pending VRAM/synchronization failures.

## Validation and limits

Interpreter and JIT ELF/DOL builds succeeded. Native suite: 188/188. Both linked PPC ELF builds passed first-image/drawing policy tests, GX flat routing/readback tests, GS bounds tests and frontend/profile checks. All 66 full-VRAM rendering signatures match the R1285 baseline. GX/cache services in PPC tests are mocked: these checks do not prove physical GPU correctness, a real Wii cold boot or improved FPS.

The observed GX-on delay beyond 600 million EE instructions is not conclusively diagnosed without its log. This release fixes the reproducible HUD detection bug and isolates experimental drawing from output. Full EE/IOP/VU JIT, textures and complex GS hardware rendering remain incomplete. No BIOS, games or raw user logs are included.

## Wii installation

Replace the app boot.dol with PCSX2-Wii-R1291-Menu-JIT.dol. Enable GX output using RIGHT, leave experimental drawing off, then boot the BIOS. Return R1291-gx-output.log after the comparison, including if no image appears.
