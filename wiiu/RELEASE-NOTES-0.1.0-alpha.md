# Gekko2 Wii U Edition 0.1.0-alpha

**This is an alpha.** Gekko2 is a PlayStation 2 emulator. The Wii U Edition is an early test port: it boots the PS2
BIOS and some games, but it is **far below full speed** and many games will not work yet. Please read the
known issues before you report a bug.

## What you need

- A Wii U with the Aroma (or Tiramisu) homebrew environment, or Cemu 2.6 or newer.
- **Your own PS2 BIOS dump** (4 MiB), made from your own console. No BIOS and no games are included.
- An SD card with this layout:

```
sd:/wiiu/apps/gekko2/gekko2-wiiu.wuhb        (Wii U; Cemu: load gekko2-wiiu.rpx)
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

## Fixed in this build

- PS2 BIOS start-up animation: the light trails were drawn with black, jagged pieces. Anti-aliased (AA1)
  lines now blend like on a real PS2, so the trails glow again.

## Reporting bugs (please do this)

1. On the main menu press **X** to switch the **verbose log ON** (the bottom bar turns green).
2. Start the game and play until the problem happens.
3. Send `sd:/wiiu/gekko2/session.log` together with a screenshot and the game name/region.

The log contains the build, the settings and speed statistics, nothing personal. Without the verbose log we
cannot help.

## Known issues

- **Speed:** most games run at roughly 0.2 % to 1 % of full PS2 speed in Cemu. Real Wii U hardware has not
  been measured yet. This alpha is about compatibility and feedback, not playability.
- **Real Wii U hardware is untested** for this exact build; it was developed and tested on Cemu 2.6.
- No sound yet.
- Picture output uses software scaling (OSScreen). The GPU presenter (GX2) is used when
  `sd:/wiiu/gekko2/present.gsh` is on the SD card.
- `gekko2-wiiu-jit.rpx` (experimental dynarec) is slower than the normal build on Cemu.

## Optional switches (files on the SD card in `sd:/wiiu/gekko2/`)

| File | Effect |
|---|---|
| `gs_async_off.txt` | GS/GIF work on the emulation thread (default: second CPU core) |
| `vu1_async_on.txt` | run VU1 on the third CPU core (experimental) |
| `fast_run_off.txt` | disable interpreted fast runs |
| `vu_jit_off.txt` | JIT build: keep VU microcode interpreted |

## License

Gekko2 is GPL-3.0-or-later. Third-party components keep their own licenses (libchdr, LZMA SDK, miniz, zstd).
PlayStation and PS2 are trademarks of Sony Interactive Entertainment. This project is not affiliated with Sony
or Nintendo.
