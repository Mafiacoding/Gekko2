# GX2 for the Wii U test port (code, not verified)

Status 2026-11-22: **written and compiled, never run.** The presenter and the probe build with devkitPPC r51 /
wut; the shaders were written from memory of the wut samples and were **never assembled**. This container has no
`latte-assembler`, no Cemu and no Wii U. Everything below tells you how to find out what works.

What GX2 can and cannot do here: it can take the finished picture (and the launcher menu), upload it as a
texture and let the **GPU scale it** to the TV and the GamePad. That replaces one `OSScreenPutPixelEx` call per
pixel. It does **not** make the emulated PS2 GS faster: the GS is still software-rendered by the CPU. A real GPU
rasterizer for the GS (like the Wii's GX path) is a separate, large project that needs shaders too.

## Way 1 - GPU presenter with your own shaders (needs latte-assembler)

1. In your Windows Project Hub (devkitPro with the `wiiu-dev` group installed) assemble the two shader sources
   into one GFD file. The exact command line depends on your tool version; run `latte-assembler --help`. The wut
   samples use the form `latte-assembler compile present.gsh present.vsh present.psh`. The vertex shader must be
   index 0 and the pixel shader index 0 in the file.
2. If the assembler rejects a file, send me the **exact error text** and I will fix the source. The syntax was
   written from memory and is the most likely thing to be wrong.
3. Copy `present.gsh` to `sdcard/wiiu/gekko2/present.gsh` (Cemu) or `sd:/wiiu/gekko2/present.gsh` (console).
4. Run `gekko2-wiiu-gx2probe.rpx`. With the file present it runs test A: colour bars, a grey ramp, a moving
   orange box, 4:3 inside the 16:9 picture, nearest filter for 5 s then bilinear. The log
   `sd:/wiiu/gekko2/gx2probe.log` has "about to ..." before risky calls, so a crash still leaves the last step.

## Way 2 - shader-free hardware scaling (no latte-assembler)

Run `gekko2-wiiu-gx2probe.rpx` **without** `present.gsh` on the card: test B sets a 640x480
(`GX2_TV_RENDER_MODE_STANDARD_480P`) TV scan buffer, writes the same pattern with the CPU and lets the console's
video output scale it. The scan-buffer pixel layout and tiling are assumptions (RGBA, pitch 640); a garbled or
black picture is a useful result, send me a screenshot and the log.

## Files

- `shaders/present.vsh`, `shaders/present.psh`: Latte assembly sources (unverified).
- `src/gx2_present.c/.h`: presenter API (`gx2_present_init`, `gx2_present_frame`, `gx2_present_shutdown`) built on
  libwhb's GFD/shader helpers. Do not combine it with OSScreen in one process.
- `probe/`: the test program (`make` here; output `gekko2-wiiu-gx2probe.rpx`).

## If it works

The presenter replaces the OSScreen drawing in `app/wiiu_main.c` (frames and the 640x480 menu go through
`gx2_present_frame`; the three log lines need a small text overlay, which GX2 does not give for free).
