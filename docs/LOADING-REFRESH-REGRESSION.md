# Loading refresh regression — owner report, 19 October 2026

Owner reports flickering and Crash/Tekken no longer reaching visible boot output
with GX Hybrid. Photograph shows the new loader at elapsed 12:27. The subsequent archive is now reviewed in [the follow-up](GX-BOOT-ARCHIVE-REVIEW.md);
game-boot cause is **not isolated**.
No game or Wii flicker fix is claimed from the photograph alone.

## Reproducible frontend defect and correction

The new loading function redrew background, logo and text directly into the
single VI scanout XFB at every presentation. Slow full-screen erase/rebuild can
expose the cleared background before the same visible elements are restored.

Linked PPC regression against the delivered `33b057f` ELF fails as expected:
an identical call performs **163,353 framebuffer writes** instead of zero.
The corrected runtime `852600c` keeps static content cached and redraws only
animation / elapsed-time strips when their values change. Menus, actual guest
copies, visible console output, stage/size changes and errors invalidate the
cache. No extra full-size framebuffer or MEM1 heap allocation is introduced.
The first loading/error screen still renders fully; this is not a general
VI double-buffer implementation or proof that guest output cannot tear.

## Checks

- Actual linked PPC rendering: unchanged call produces zero XFB writes;
  animation changes only rows 290–293 at 480 lines; elapsed changes stay in
  the animation/time strips; invalidation restores the full background.
- Loading/error rendering, title visibility and bounded XFB writes tested
  at 640×480 and 640×576. Video/cache/elapsed-format platform calls are stubbed.
- 96 linked console routing cases pass, now also checking that actual visible
  console callbacks invalidate the cached screen and suppressed output does not.
- Fresh control Wii build with devkitPPC r50 / GCC16.1.0 / libogc3.1.0 retains
  SPEED/AOT/DSI/GX-fence flags. DOL/ELF identity and static MEM1 pass:
  2,335,244 bytes remain. This does not measure peak allocation or Wii FPS.
- No EE/IOP/VU/GS core or instruction/first-picture admission changes in this
  patch. The 90 previous host gates are not reported as rerun for this GUI change.

## Game regression remains open

Use the reviewed archive and any available exact session/build identity, active CPU/GX options, CPU PCs and
retirement, display registers, GIF/VIF/VU counts, first-image and GX fault data
to distinguish a guest boot stall, excessive host presentation cost, a failed
first-image probe or GX failure. Compare with the earlier owner evidence.
Do not force first-image success, skip guest interrupts, change boot timing or
relax GPU synchronization based solely on the loading photograph.

No new physical Wii/Dolphin execution, no public alpha release and no
Crash/Tekken boot success is asserted. Keep the existing rollback SD package.

[Synthetic verification record](verification/LOADING-REFRESH-RESULTS.json).
