# Gekko2 Wii U Edition 0.1.0-alpha (build 19, test build)

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

**GS core switch (ZR in the main menu, only with the GPU renderer on):** GX2 can only be used from the CPU core
that started it (core 1). With `GS CORE: SHARED` the PS2 graphics work runs together with the emulation on core 1.
With `GS CORE: OWN` the emulation moves to core 2 and core 1 does the graphics work, GX2 and the picture output, so
graphics get their own core again. Core 2 has a smaller cache (512 KB instead of 2 MB), so it is not clear which
is faster: please test both and send logs. Saved as `sd:/wiiu/gekko2/gx2_split_on.txt`.

## Fixed in this build

- **GPU renderer on Cemu (build 13):** build 12 made the picture worse in Cemu (horizontal streaks): Cemu works out
  the row length of these textures itself and ignored the padded rows build 12 used. Build 13 removes the padding
  and gives every texture upload fresh memory from a ring buffer instead, so Cemu always sees new data. This also
  removes a wait for the GPU on every texture change (helps real hardware too; not measured yet).
- **Build 19: second Namco logo on Cemu fixed.** With the GPU renderer on Cemu, Tekken showed its "PRODUCED BY
  namco" logo a second time lower in the picture on every other field. Cause: Cemu sometimes drew an older upload
  of picture rows that had the same size and the same memory address (Cemu checks such textures only sparsely
  for changes). On Cemu the rows are now uploaded as flat-coloured runs instead of a texture. Checked with raw frame
  dumps in Cemu 2.6: normal app 0 of 106 frames with a second logo (before: about half), JIT app 0 of 201;
  same speed. A real Wii U keeps the texture upload. Switches: `upload_rle.txt` / `upload_tex.txt`.
- **Build 18: techniques from the PPSSPP Wii U port** (jereif/PPSSPP_wiiu, GPL-2.0-or-later; re-implemented, not
  copied), each with an off switch:
  - GX2 state cache: shaders and scissor are only sent to the GPU when they change. Cemu, Tekken boot, normal
    app with GPU: 384 million PS2 instructions after 140 s, 366 million in build 16.
  - `dcbz` when filling upload memory (textures, picture rows, depth rows): saves memory reads on a real Wii U;
    no measurable change on Cemu. Off: `dcbz_off.txt`.
  - Tiered JIT (compile a block only after it ran a few times): on Cemu with the full 6 MB code cache it was
    about 5% slower, so it only switches on when the code memory is small (RWX heap below 3 MB). Force on:
    `jit_tier.txt`, off: `jit_tier_off.txt`.
  - Not taken over: PPSSPP's register cache, fused loads/stores and peephole passes assume a JIT that runs
    whole blocks freely; Gekko2's PS2 JIT checks every instruction for exact interrupts and memory faults, which
    those passes would remove. Paired singles for the VU: not yet, needs exact PS2 float rounding first.
- **Test build 17, JIT app on a real Wii U: RWX code memory (please test).** Under Aroma the JIT app now looks for
  read/write/execute memory in the range 0x00800000-0x01000000 (technique from the PPSSPP Wii U port by jereif,
  GPL-2.0-or-later, re-implemented). Generated code there needs no write/execute switching and may run on any
  CPU core, so the emulator and the GPU renderer run together on the main core (2 MB cache), as on Cemu. The
  verbose log shows `JIT RWX heap: ...` with the result. If the JIT app crashes or misbehaves, create
  `sd:/wiiu/gekko2/jit_codegen.txt` to go back to the official code area and please send the log. Untested on
  hardware; no speed claim.
- **Cemu, JIT app:** the emulator runs on the main core there as well (Cemu does not enforce the code core):
  516 million PS2 instructions after 290 s with the GPU renderer, about 450 million before (local Cemu 2.6).
- **GPU renderer: misplaced picture pieces (build 16):** Tekken's "PRODUCED BY namco" logo was drawn in pieces at
  several wrong places with the GPU renderer. The texture memory ring from build 13 starts over when it is full,
  and a texture that was still cached could then point at memory that already held other data. Cached textures
  are now uploaded again after the ring starts over. Reproduced and checked in Cemu 2.6: the logo is drawn
  correctly again.
- **Speed, normal app (build 15):** on an SD card without `optimization.cfg` (every new install) the default
  settings switched off the interpreter's fast path even though the normal app has no JIT. Fixed. Measured in Cemu
  2.6 (software GPU on a Linux PC, Tekken Tag boot, 86 s): 227 million PS2 instructions with the GPU renderer on,
  177 million before the fix, 165 million with the GPU renderer off. Not yet measured on a real Wii U.
- **JIT app on Cemu fixed (build 15):** the BIOS hang from build 13 is found and fixed. Cemu's own recompiler gets
  one PowerPC carry case wrong (the carry out of a chained `subfe`), which inverted Gekko2's 64-bit compares
  (`slt`/`slti`) once Cemu had compiled them. The Wii U JIT now uses an equivalent sequence without that chain.
  The fast Cemu mode is on again by default (`jit_cemu_safe.txt` turns it off).
- **GPU renderer in the JIT app (build 15):** the JIT app can now use the GPU renderer as well (`-` in the menu); the
  PS2 graphics work then runs on the main core. Cemu, Tekken Tag boot, 86 s: 136 million PS2 instructions (JIT +
  GPU) vs. 98 million (JIT in build 14). In Cemu the normal app is still faster than the JIT app; real Wii U
  numbers for the JIT app are still missing.
- **GS core switch:** in Cemu `GS CORE: SHARED` was faster than `OWN` (227 vs. 192 million instructions in 86 s).
- **JIT app on Cemu (build 14):** the build 13 change ran the JIT app about 3.5 times faster in Cemu (2.7 instead of
  0.3-0.8 million PS2 instructions per second), but the PS2 BIOS then stayed in its timer calibration loop and
  Tekken never loaded. It is now off by default; create `sd:/wiiu/gekko2/jit_cemu_fast.txt` to test it.
- **JIT app on Cemu (build 13):** Cemu switches off its own recompiler for the JIT code area as soon as an app
  switches that area between write and execute mode, so the JIT code ran in Cemu's slow interpreter. When Gekko2
  runs as a directly loaded RPX in Cemu, it no longer switches the area. Real Wii U behaviour is unchanged.
  Not yet measured on Cemu.
- **GPU renderer on Cemu (build 12):** build 11 fixed the probe (9 of 9 checks), but games still showed artifacts.
  Cemu checks texture memory for changes only once per frame and does not see CPU writes into the GPU's picture,
  so textures that change several times per frame and picture parts drawn by the software renderer were lost.
  Uploads now go through textures that Cemu re-checks immediately, and CPU-drawn picture rows are copied onto the
  GPU picture with a draw. In the PC model of the GPU the BIOS intro still matches the software renderer, with
  and without depth. Not yet confirmed on Cemu or a real Wii U. The probe now also checks these two cases
  (`second texture, same frame` and `CPU row upload`) and logs details if depth is unavailable.
- **GPU renderer (build 11):** Cemu showed heavy artifacts. The GX2 probe found two causes: textured draws came
  out black (the texture description was changed after it had been handed to the GPU), and depth could not be
  read back (Cemu returns zeros for the old copy path). Textures now use a separate, fixed texture view, and
  depth is read back through a small shader draw when the copy path returns nothing. Not yet confirmed on Cemu
  or a real Wii U: please run `gekko2-wiiu-gx2probe.rpx` once and send `sd:/wiiu/gekko2/gx2probe.log`.
- **Hitman 2 boot hang:** its SYSTEM.CNF has a blank after the file name (`cdrom0:\SLUS_203.74;1 `); Gekko2 kept
  the blank and looked for a file that does not exist. Hitman 2 now loads its program and game data; it still
  stops before the first picture (being investigated).
- **JIT app:** two reasons why the recompiler never ran a single native block (tester logs: `blocks=0`): the
  interpreted fast path ran first (fixed in build 8), and the native-block option was off in the saved option
  mask (fixed in build 9). The verbose log now has `# jitb` lines that show why a block was not used.
  Real Wii U results needed.
- **Exit with software picture output (`gx2_off.txt`):** the app hands the screen back before the Wii U Menu
  takes over; exiting froze on a tester's console.
- **Logs for loading hangs:** the verbose log now records the disc image size and every disc command and sector
  read (`# cdvd` lines), so a hang shows the last disc access.
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
| `gx2_split_on.txt` | GS on its own core with the GPU renderer (same as ZR in the menu) |

## License

Gekko2 is GPL-3.0-or-later. Third-party components keep their own licenses (libchdr, LZMA SDK, miniz, zstd).
PlayStation and PS2 are trademarks of Sony Interactive Entertainment. This project is not affiliated with Sony
or Nintendo.
