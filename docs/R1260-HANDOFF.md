# R1260: GS homogeneous STQ correction

Priority remains OSDSYS correctness, then measured speed, then Tekken.
R1259 fresh Interpreter boot reached the actual Browser / System Configuration menu.

Triangle rasterization incorrectly interpolated reciprocal Q and S/Q.
That cancelled constant Q and yielded the wrong mapping for varying Q.
Interpolate raw S,T,Q in screen space, then divide S,T by interpolated Q.
The bundled primary reference source confirms this in
`docs/reference/pcsx2/pcsx2/GS/Renderers/SW/GSDrawScanline.cpp`:
DrawScanline initializes s,t,q separately; SampleTexture divides s/q,t/q;
Step increments each independently. The sprite path already divides by Q.
The zero-Q defensive fallback remains zero coordinates.

Meaningful rendered-pixel tests cover varying Q and constant Q=2.
Both fail against the old renderer and pass with the correction.
All 165 native regressions pass. Both Wii engines built successfully and
passed all 96 existing PowerPC helper checks per engine. Additionally, three rendered STQ triangle pixels per engine were checked
in the emitted PowerPC code: varying Q, constant Q=2 and Q=1, with both
S and T coordinates. The shared helper runner now verifies function return, resets the host
stack per call and uses a 10M-instruction budget instead of 10K.
All 99 checks per engine also pass with this strengthened runner.
These are not Dolphin or end-to-end BIOS tests.

The change also removes three vertex reciprocal divisions per textured
triangle pixel. No measured Wii FPS increase is claimed.
Nearest sampling, lost fractional UV and missing TEX1 linear filtering
remain open; this revision does not claim a fully correct OSDSYS orb.
NVRAM persistence, checkpoint PAD bindings and deliberately timed Back /
Browser input tests also remain open. Do not force BIOS completion flags.

Native A/B continuation starts from the same R1259 menu checkpoint and runs
30 million further EE instructions without pressing keys. The diagnostic
restores host PAD bindings from the recorded successful INIT/OPEN trace,
after checking the guest opened-port structure, as documented in R1259.
No guest PC, RAM or registers are forced. This is not a fresh R1260 boot.

A/B result: both runs remain unhalted and display Browser / System
Configuration. The visible difference is confined to the orb region
(bounding rectangle x=160..371, y=120..329; 17,722 output pixels differ).
The corrected orb is still fragmented; filtering and other GS issues
need further investigation. Final EE counts differ by 41 instructions
within the bounded slice; no assertion of complete lockstep is made.
