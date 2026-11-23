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

## Build flags and speed (measured)

- **Default: `-O1` plus the software TLB translation cache** (`GEKKO2_OPT_FASTMEM`, enabled in `app/g2_app.c`).
  Host x86, same BIOS, two runs in parallel: 371.6 s -> 278.9 s to the first BIOS menu with an identical picture.
  gprof: `ee_tlb_translate_selected` 18.6% -> 7.7% of the time. Cemu (owner screenshots, rough): about 15-20% less
  time to the same EE instruction count; digits in the screenshots were partly cut off.
- **`-O2` is shelved.** On the host `-O2` is ~20% faster than `-O1` (267 s vs 336 s), but with devkitPPC GCC 16.2 in
  Cemu: (1) with `-O2` only for `ee_hle_thread.c` the EE hangs in `WaitSema` (EE pc `0x83624`); `-O2` only for
  `ee_core.c` runs on; everything `-O2` except `ee_hle_thread.c` runs on; (2) `-O2` builds show thin vertical streaks in
  the BIOS picture even with `-ffp-contract=off` (so it is not fused multiply-add); (3) no measurable speed-up in
  Cemu (`ips` about 1.2 M/s either way). Root causes are unknown. `make O2_FILES=... O1_FILES=... O2_EXTRA=...`
  reproduces the test builds.
- The EE dynarec build (`make JIT=1`) is slower than the interpreter in Cemu (about 1.5x longer to the same
  instruction count) because Cemu has to translate the generated code a second time.

## Measuring a game: `session.log` and `vb/s`

Every second the status line shows `ips=` (EE instructions/s) and `vb/s=` (guest VBlanks per second) with a percentage:
the core counts NTSC frames, so 59.94 vb/s = 100% of a real PS2. While emulating, one line per second is appended to
`<sd>/wiiu/gekko2/session.log` (`t_s,ee_instr,ips,vblank_total,vblank_per_s,frames_presented,pad_mask`), with a header
naming the BIOS, the disc and the option mask. `pad_mask` is the PS2 button mask the app sent, so you can see that input
reached the core. See `docs/WIIU-GS-GPU-FEASIBILITY.md` for why speed, not the GPU, comes first.

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

## GS worker thread (experimental)

`g2_config.gs_async` (on by default in the Wii U app; create `<sd>/wiiu/gekko2/gs_async_off.txt` to turn it off) runs the
GIF/GS work on a second Espresso core (core 2, or 1 if the emulation thread is on 2). The emulation thread copies GIF
quadwords into one ring per path and returns; the worker executes the unchanged synchronous GIF code on the oldest entry
that can make progress (`source/hw/gif_async.inc`). GS status reads (CSR, GIF_STAT) do not wait and may lag, like polling
real hardware; VRAM readback, GIF control writes, checkpoints and the presenter drain first. Wii code is not touched.

Host measurement (real BIOS to the same reference frame, two runs each, 4 cores, BIOS boot only):
sync 348-354 s, worker 324-327 s (about 8 % faster). Worker busy ~103 s, emulation thread waited ~17 s in drains.
The final frame hash matched the synchronous run every time. This is a host result for the BIOS boot; Cemu and real Wii U
timing, and games, are not measured. The session log prints `GS worker: ...` counters at exit.

## Interpreted fast runs (interpreter build)

`g2_config.fast_run` (on by default; create `<sd>/wiiu/gekko2/fast_run_off.txt` to disable; ignored in the JIT build)
runs up to 96 guest instructions per call on the live register file: integer ALU, RAM loads/stores and non-likely
branches with their delay slot. Before a run, the guards of the native-window admission (pending interrupts, timers,
display phase, Count/Compare, DMA/VU/GIF activity, special PCs) bound how many instructions are free of events;
the retirement is then published in bulk. Anything else (MMIO, COP0, MULT/DIV, FPU, MMI, likely branches) ends the run and
the scalar interpreter executes it as before. Code: `ee_iw_*` in `source/core/ee/ee_core.c`, `GEKKO2_WIIU` only.

Host verification (real BIOS, clock frozen because the BIOS RTC reads the host clock): the scalar path is deterministic
(two runs identical) and the fast-run path gives identical hashes over GPRs, COP0 and the whole EE RAM at 150M, 267M, 385M
and 501M EE instructions, in both the 8-slot scheduler and the event scheduler. 74-77 % of those instructions ran inside
runs. Speed on the host for that BIOS animation phase: 68.3 s -> 61.5 s (8-slot scheduler), 64.8 s -> 58.0 s (event
scheduler), about 10 %. That phase is also GS/IOP heavy, so the EE share is only part of the time. Not measured: Cemu,
real Wii U, any game. The Wii code is unchanged (disassembly diff of the Wii ELF against the previous commit).

## IOP worker thread (experimental, off by default)

`g2_config.iop_async` (create `<sd>/wiiu/gekko2/iop_async_on.txt`) runs the IOP catch-up of the event scheduler on the
third Espresso core while the EE runs its next window; `event_sched_on.txt` enables the event scheduler alone.
Ownership rules are in `include/core/iop_async.h`: the 34 functions on the EE/IOP border (found from the object files'
symbol tables: SIF, SIF0/SIF1 DMA, EE INTC, CDVD/HLE file and load protocols, IOP heap, pad, memory card, CDVD config)
wait either for the IOP job (EE side) or until the EE is parked at a fixed point (IOP side). The worker is recognised
by its stack range.

Host result (real BIOS, clock frozen, 501M EE instructions): the worker gives exactly the same EE state, EE RAM and IOP
RAM hashes as the synchronous event scheduler, but it is slower (53-58 s against 44-45 s). The IOP's own event horizon
(timers) cuts windows so short that a job averages 4 IOP ticks, and 20-25 million hand-overs cost more than the overlap
saves. That is why it stays off. The event scheduler alone was about 5 % faster than the 8-slot scheduler on the host
(46.5 s against 49.2 s) and is also off by default.
