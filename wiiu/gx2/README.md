# GX2 for the Wii U test port (code, not verified)

Status 2026-11-23: **Test A (GPU presenter) was run by the owner in Cemu 2.6 (Vulkan): colour bars in the right order,
moving box, a steady 61 frames/s in the probe log.** Test B (shader-free scan buffer) has not been run. Not tested on a Wii U.
Earlier status: written and compiled, never run. The presenter and the probe build with devkitPPC r51 /
wut; the shaders were written from memory of the wut samples and were **never assembled**. This container has no
`latte-assembler`, no Cemu and no Wii U. Everything below tells you how to find out what works.

What GX2 can and cannot do here: it can take the finished picture (and the launcher menu), upload it as a
texture and let the **GPU scale it** to the TV and the GamePad. That replaces one `OSScreenPutPixelEx` call per
pixel. It does **not** make the emulated PS2 GS faster: the GS is still software-rendered by the CPU. A real GPU
rasterizer for the GS (like the Wii's GX path) is a separate, large project that needs shaders too.

## Way 1 - GPU presenter with your own shaders (needs latte-assembler)

1. `latte-assembler` is **not** part of devkitPro; it belongs to decaf-emu. Download the Windows artifact from the
   latest successful workflow run at https://github.com/decaf-emu/decaf-emu/actions (a GitHub login is needed for
   artifacts) and put `latte-assembler.exe` on your PATH or into `<devkitPro>/tools/bin`. `tools/build_all.sh`
   then assembles the shaders with `latte-assembler assemble --vsh=present.vsh --psh=present.psh present.gsh`
   (older builds call the subcommand `compile`; `build_all.sh` tries both). The vertex shader is index 0 and the
   pixel shader index 0 in the file. The vertex shader follows the verified example in
   https://github.com/GaryOderNichts/wiiu-shaders (shaders.md); the pixel shader's texture sampling is unverified.
2. If the assembler rejects a file, send me the **exact error text** and I will fix the source. The pixel shader
   is the most likely thing to be wrong.
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

## In the app (since this commit)

`gekko2-wiiu.rpx` / `gekko2-wiiu-jit.rpx` look for `<sd>/wiiu/gekko2/present.gsh` at start. If it loads, the launcher menu and the
emulator picture go through `gx2_present_frame` (the CPU composes 640x480 with the last three log lines burned in, the GPU
scales it with bilinear filtering to the TV and the GamePad). If the file is missing or rejected, the app uses OSScreen as
before and says so in the log. The two display paths exclude each other. Not yet run in Cemu with the full app.
