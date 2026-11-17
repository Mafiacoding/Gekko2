# Gekko2 on Wii U / Cemu — test port (branch `codex/wiiu-cemu-test`)

Status 2026-11-17: **experimental test, not a release, no speed claim.** Built with devkitPPC r51 /
GCC 16.2.0 / wut. Owner-observed on **Cemu 2.6 (Windows)**: the app starts, finds the SD card and a user BIOS,
runs the interpreter and shows the BIOS boot animation (dark blue background with coloured lights).
Not tested on a physical Wii U. The same BIOS on the Linux host shows the "Sony Computer Entertainment" logo.

**Build flags matter.** With `-O2` the app froze the EE in a `WaitSema` syscall (EE pc `0x83624`, EE
instructions stuck for 36 minutes) while the host and a 32-bit big-endian PowerPC build (qemu-user) ran on.
With `-O1 -fno-strict-aliasing -fwrapv` it runs on, and owner-observed on Cemu 2.6, plain `-O1` (test build C)
also runs cleanly, so `-O1` is the default (it matches the Wii build). Whether `-O2` plus only
`-fno-strict-aliasing` (A) or only `-fwrapv` (B) also works was not reported; the cause in the core is unknown. ASan/UBSan on the host found no memory error up to 106 M EE instructions
(one left-shift-of-negative report at `ee_core.c:10685`, not investigated).

The Wii build uses libogc, GX, IOS and the ARM worker, none of which exist on the Wii U, and Cemu cannot run
Wii/vWii DOLs. This directory is a separate Wii U front end around the *portable* Gekko2 core.

## What exists

| Step | Content | Evidence |
|---|---|---|
| 1 Feasibility | `hello/`: minimal wut program → `.rpx`/`.wuhb` | builds; RPX is ELF32 big-endian PowerPC, ABI `EABI_CAFE` (verified) |
| 2 Core + video | `app/g2_app.c` (platform-neutral loop: BIOS load, `system_init`, `system_run_interleaved`, GS display read-out), `app/wiiu_main.c` (wut: OSScreen TV/GamePad, SD path), `Makefile` | all 87 core sources + libchdr compile for Espresso in **host mode** (no `GEKKO`, no libogc, no AOT/JIT → **interpreter only**); full app links to `gekko2-wiiu.rpx`/`.wuhb` (verified) |
| 3 BIOS boot | run on Cemu 2.6 | owner screenshots: runs, boot animation visible with `-O1` (also with `-fno-strict-aliasing -fwrapv`); `-O2` build hung |
| 4 GamePad input | `poll_pad` in `app/wiiu_main.c` (VPAD) | builds; **input not yet tested** |

Host smoke test (`wiiu/tools/host_smoke.sh`): builds the same loop natively and runs it with a *synthetic*
ROMDIR BIOS (no Sony firmware): 5 batches, EE/IOP counters advance, a missing BIOS fails cleanly
(`PASS g2_app host smoke`). This is x86 little-endian; it proves the loop, not big-endian behaviour.

## Experimental: EE dynarec in the Wii U codegen area (`make JIT=1`)

Owner-observed in Cemu 2.6 with `jitprobe/`: the app has a 16 MiB codegen area at `0x02018000`, usable only from
core 0; code in the normal heap crashes Cemu; `OSCodegenCopy` and RW/R_X switching both run generated PowerPC.
`make JIT=1` (output `gekko2-wiiu-jit.rpx`) builds the Wii EE dynarec for that area:

- `include/core/recompiler/native_ppc.h` defines `GEKKO2_NATIVE_PPC` (true for the Wii build and `GEKKO2_WIIU_JIT`);
  the dynarec files test that instead of `GEKKO`. The Wii DOL is byte-identical before and after this change.
- The code cache lives in the codegen area (`ppc_code_cache.c`), R_X normally; alloc/release/trim and the copy in
  `finalize()` switch to RW through `source/core/recompiler/wiiu_codegen.c`.
- The emulator runs on a thread pinned to the codegen core; the main thread (core 1) keeps ProcUI, VPAD and the screen.
- If the area is unavailable the app says so and runs the interpreter. Only the EE dynarec is enabled
  (IOP/VU dynarec stay interpreters). Real Wii U hardware is **not** verified (needs `cos.xml` codegen permission).
- Differential test (Linux 32-bit big-endian PowerPC under qemu-user, CPU 750, mmap/mprotect standing in for the
  codegen area): see `docs` notes in the commit message for the result; this is not a Cemu or Wii U measurement.

## Not done / known limits

- No audio, no GX2: video is a software frame copied through `OSScreen` about once a second. GamePad is
  mapped B=Cross, A=Circle, Y=Square, X=Triangle, L/R=L1/R1, ZL/ZR=L2/R2, +=Start, -=Select, sticks=analog
  (HOME stays with the OS). Nothing else (Pro Controller, Wiimote) is read.
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
make OPT="-O2 -fno-strict-aliasing"   # flag experiments (default OPT: -O1)
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
