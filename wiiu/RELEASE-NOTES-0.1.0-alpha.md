# Gekko2 Wii U Edition 0.1.0-alpha (build 4)

**This is an alpha.** Gekko2 is a PlayStation 2 emulator. The Wii U Edition is an early test port: it boots the PS2
BIOS and some games, but it is **far below full speed** and many games will not work yet. Please read the
known issues before you report a bug.

## What you need

- A Wii U with the Aroma (or Tiramisu) homebrew environment, or Cemu 2.6 or newer.
- **Your own PS2 BIOS dump** (4 MiB), made from your own console. No BIOS and no games are included.
- An SD card with this layout:

```
sd:/wiiu/apps/gekko2/gekko2-wiiu.wuhb        (Wii U; Cemu: load gekko2-wiiu.rpx)
sd:/wiiu/apps/gekko2-jit/gekko2-wiiu-jit.wuhb (optional: experimental recompiler build, see below)
sd:/wiiu/gekko2/bios/ps2bios.bin             (your BIOS dump)
sd:/wiiu/gekko2/games/                       (your games: .iso, .bin/.cue raw images, .chd)
```

## Supported disc images

- ISO (2048-byte sectors) and raw BIN images (2352-byte sectors, MODE1/MODE2).
- CHD: DVD CHDs (`chdman createdvd`) and CD CHDs (`chdman createcd`), all CHD codecs (zlib, LZMA, zstd,
  FLAC, Huffman). CD CHDs with extra audio tracks are accepted; the PS2 data track is used.

## Controllers

All of these work at the same time and drive PS2 pad 1:

- Wii U GamePad, Wii U Pro Controller, Classic Controller / Classic Controller Pro,
  Wii Remote (held sideways) and Wii Remote + Nunchuk.
- Default layout (Nintendo positions): B = Cross, A = Circle, Y = Square, X = Triangle, L/R = L1/R1,
  ZL/ZR = L2/R2, + = Start, - = Select, stick clicks = L3/R3.
- **Remap the buttons in the menu:** press **Y** on the main page, choose a PS2 button, press A, then press the
  controller button you want. The mapping is saved to `sd:/wiiu/gekko2/controls.cfg`.
- Back to the menu while a game runs: hold **L + R + ZL + ZR** and press **+**.

## New in build 4: GPU renderer (GX2), opt-in

Press **-** in the main menu (bottom line: `GPU RENDERER ON/OFF`) to let the Wii U GPU draw the PS2 graphics (triangles and
sprites with textures, blending and depth). Draws the GPU path cannot reproduce exactly still use the software
renderer. While it is on, the PS2 graphics work runs on the emulation thread instead of the second CPU core
(GX2 can only be used from one core), and games that copy their frame buffer into itself cost extra time.
In our PC model of the GPU the picture matched the software renderer (BIOS intro and Tekken Tag intro), but in
2D-heavy scenes it was slower. Whether it is faster on a real Wii U, especially in 3D scenes, is exactly what we
need to find out: please run the same game with the GPU renderer on and off (verbose log on) and send both logs.
The switch is saved (as `sd:/wiiu/gekko2/gx2_gs_on.txt`). It has no effect in the JIT app.

## Fixed in this build

- **HOME button:** the Wii U HOME menu works during the menu and while a game runs; "Close software" returns
  to the Wii U Menu without crashing. "Exit" in the Gekko2 menu also returns to the Wii U Menu.
- **GamePad:** the picture is now always shown on the GamePad as well (no TV needed). With the verbose log on,
  the last log lines are drawn over the bottom of the picture.
- **GPU output (GX2):** the presenter shaders are now built in, so the picture is scaled by the GPU on TV and
  GamePad. If you get a black screen, create `sd:/wiiu/gekko2/gx2_off.txt` to use the old software output.
- **Disc diagnostics:** the log now records the image size and any disc read errors.
- PS2 BIOS start-up animation: the light trails were drawn with black, jagged pieces. Anti-aliased (AA1)
  lines now blend like on a real PS2, so the trails glow again.

## Experimental JIT build (please test on a real Wii U)

`Gekko2 Wii U Edition JIT` translates PS2 code into native Wii U code instead of interpreting it. On Cemu it is
slower than the normal build, but Cemu handles generated code very differently from the real console, so we
need measurements from real hardware. Please run the same game with both apps (verbose log on) and send both
`session.log` files.

## Reporting bugs (please do this)

1. On the main menu press **X** to switch the **verbose log ON** (the bottom bar turns green).
2. Start the game and play until the problem happens.
3. Send `sd:/wiiu/gekko2/session.log` together with a screenshot and the game name/region.

The log contains the build, the settings and speed statistics, nothing personal. Without the verbose log we
cannot help.

## Known issues

- **Speed:** very slow. A tester's real Wii U log (Tekken Tag, first build) shows about 0.4 emulated frames per
  second (under 1 % of full speed); after 5 minutes Tekken Tag was still on the "Produced by Namco" screen.
  This alpha is about compatibility and feedback, not playability.
- The HOME/GamePad/GX2 fixes and the GPU renderer were built and checked for compile errors, but not yet run
  on a real Wii U or Cemu.
- No sound yet.
- **Games larger than 4 GiB:** FAT32 SD cards cannot hold files of 4 GiB or more; such an ISO is cut off and
  the game hangs while loading. Convert it to CHD (`chdman createdvd`).
- Some games hang while loading (for example Hitman 2 on one tester's console). Please send the verbose log.
- The JIT build is slower than the normal build on Cemu; real Wii U results are still missing.

## Optional switches (files on the SD card in `sd:/wiiu/gekko2/`)

| File | Effect |
|---|---|
| `gs_async_off.txt` | GS/GIF work on the emulation thread (default: second CPU core) |
| `vu1_async_on.txt` | run VU1 on the third CPU core (experimental) |
| `fast_run_off.txt` | disable interpreted fast runs |
| `vu_jit_off.txt` | JIT build: keep VU microcode interpreted |
| `gx2_off.txt` | software picture output (OSScreen) instead of the GPU (also turns the GPU renderer off) |
| `gx2_gs_on.txt` | GPU renderer on (same as the - switch in the menu) |

## License

Gekko2 is GPL-3.0-or-later. Third-party components keep their own licenses (libchdr, LZMA SDK, miniz, zstd).
PlayStation and PS2 are trademarks of Sony Interactive Entertainment. This project is not affiliated with Sony
or Nintendo.
