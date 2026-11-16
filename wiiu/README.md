# Gekko2 on Wii U / Cemu — test port (branch `codex/wiiu-cemu-test`)

Status 2026-11-16: **experimental test, not a release, no speed claim.** Built and linked with devkitPPC r51 /
GCC 16.2.0 / wut; **not yet run on Cemu or a Wii U by the author** (no Cemu in the build container; Cemu
downloads were blocked). Everything marked "verified" below was checked on a Linux host only.

The Wii build uses libogc, GX, IOS and the ARM worker, none of which exist on the Wii U, and Cemu cannot run
Wii/vWii DOLs. This directory is a separate Wii U front end around the *portable* Gekko2 core.

## What exists

| Step | Content | Evidence |
|---|---|---|
| 1 Feasibility | `hello/`: minimal wut program → `.rpx`/`.wuhb` | builds; RPX is ELF32 big-endian PowerPC, ABI `EABI_CAFE` (verified) |
| 2 Core + video | `app/g2_app.c` (platform-neutral loop: BIOS load, `system_init`, `system_run_interleaved`, GS display read-out), `app/wiiu_main.c` (wut: OSScreen TV/GamePad, SD path), `Makefile` | all 87 core sources + libchdr compile for Espresso in **host mode** (no `GEKKO`, no libogc, no AOT/JIT → **interpreter only**); full app links to `gekko2-wiiu.rpx`/`.wuhb` (verified) |
| 3 BIOS boot | run on Cemu | **not verified** (see "Please test") |

Host smoke test (`wiiu/tools/host_smoke.sh`): builds the same loop natively and runs it with a *synthetic*
ROMDIR BIOS (no Sony firmware): 5 batches, EE/IOP counters advance, a missing BIOS fails cleanly
(`PASS g2_app host smoke`). This is x86 little-endian; it proves the loop, not big-endian behaviour.

## Not done / known limits

- No audio, no input (pad/Wiimote), no GX2: video is a software frame copied through `OSScreen` once a second.
- No AOT/JIT/dynarec: the PPC dynarec would need a Wii U code-memory strategy (unverified).
- The core is compiled without `GEKKO`; some paths differ from the Wii build and have **not** been checked on
  big-endian hardware/emulation. Expect bugs; the interpreter will be very slow.
- SD/file access uses plain `fopen` on `<sd>/wiiu/gekko2/...` with three path spellings; whether Cemu exposes
  `/vol/external01` as its `sdcard` folder is from memory, not tested.
- Warnings are suppressed (`-w`) for this test build; third-party sources are compiled unchanged.

## Build (Windows Project Hub or Linux)

Needs devkitPro with `wut` (`pacman -S wiiu-dev`). Environment: `DEVKITPRO`, `DEVKITPPC=$DEVKITPRO/devkitPPC`.

```sh
cd wiiu
make -j4          # gekko2-wiiu.rpx
make wuhb         # gekko2-wiiu.wuhb
make -C hello     # step-1 probe
bash tools/host_smoke.sh   # optional native smoke test (Linux, gcc)
```

## Please test (step 3) — what to do and what to send back

1. Put **your own** PS2 BIOS dump (4 MiB) at `<Cemu sdcard folder>/wiiu/gekko2/bios/ps2bios.bin`
   (on a real Wii U: `sd:/wiiu/gekko2/bios/ps2bios.bin`). Optional: a disc image at `.../wiiu/gekko2/game.iso`.
   Never share BIOS, ISOs or logs with private paths.
2. Load `gekko2-wiiu.rpx` (or the `.wuhb`) in Cemu. Enable Cemu's debug log (OSReport output) if you can.
3. The TV/GamePad screen shows status lines: SD mounted?, BIOS path tried, `BIOS ok: <version>`, then one
   line per second `t=…s batches=… EE pc=… instr=… IOP instr=…`. A frame appears once the guest enables the GS display.
4. Send me: whether Cemu starts it at all, the last screen text (screenshot), the Cemu log lines starting with
   `Gekko2`/`t=`, the `.rpx` SHA-256, Cemu version and your OS. Mention freezes, crashes or an unresponsive HOME menu.
