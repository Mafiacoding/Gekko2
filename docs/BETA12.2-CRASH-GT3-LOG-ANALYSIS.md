# Gekko2 SPEED BETA12.2 — Crash E3 and GT3 field-log continuation

Test checkpoint: 2026-10-14. This is **not** a confirmed gameplay fix.

## Log evidence (supplied private Wii logs; not redistributed)

**Likely Crash Bandicoot: The Wrath of Cortex E3 Demo** — `R1389-SPEED-BETA12` with GX requested.
- At 755,368 ms: 475,182,876 EE instructions, PC 0x001F23F8, `FIRST_IMAGE=0`, `PMODE=0`; no scanout.
- 1,993 ISO sector-cache requests, 148 fills, 0 failures. CHD not used.
- EE native `EE_BLOCK.retired` plateaued at 13,538,805, and native-window retirements at 8,473,276 after ~46 seconds, while guest EE instruction counter kept increasing.
- Source inspection found `ee_core_block_pointer` refusing physical code addresses below 0x00200000. The Crash E3 disc ELF begins at 0x00100008 and the later observed PC is around 0x001Fxxxx, explaining this JIT regression. This does **not** prove the missing display is solely caused by JIT refusal.
- `GS_ROUTE.draws=1` and `PAD_INPUT.serial_commands=0`; the graphics and device paths require future investigation.

**Likely Gran Turismo 3** — `R1389` in software mode.
- First image at 64,984 ms / EE 64,000,224, with GS 640x512, `PMODE=66`.
- Exit at 4,257,684 ms (70m 57.7s), EE 3,517,091,517, PC 0x0101EED0.
- ISO reads stopped at 330 cache requests / 40 fills, 0 failures; GS draw count reached 1,396 then stopped rising.
- `PAD_INPUT.samples=707` but `serial_commands=0` in the SIO2 path. This is not proof of a memory-card-specific blocker; the current EE FILEIO path returns -ENODEV for `mc0:/mc1:` and the simulated PS1-style 128 KiB card is not a complete PS2 MagicGate card.
- There is an extreme code-arena fragmentation signal: 772,864 bytes free but largest contiguous allocation only 5,280 bytes at exit (709 holes). The log does **not** prove this caused the GT3 stall, but fragmentation and repeated SIF/IOP RAM synchronization deserve profiling.

## BETA12.2 experimental code changes

1. Expand guarded EE block-peek admission from physical RAM >=2 MiB to >=1 MiB, keeping the lowest 1 MiB on the conservative scalar path. All live mapping, generation, memory, exception and instruction checks still apply. Does not force execution of an unsupported instruction in PPC.
2. Add low-impact `EE_SCALAR_HOT samples=... pc:hits ...` diagnostic line in 5s progress logs. Samples only scalar fallback attempts at a prime-number stride (4093), so it helps identify game wait PCs that the existing JIT-only `EE_HOTPC` cannot see. It does not automatically skip loops or change guest state.
3. Add host guard regression test verifying KUSEG and KSEG0 1–2 MiB native code sources, exclusion of the lower 1 MiB, and rejected invalid/changed TLB mappings.

## Build/test status

- `test_ee_low_elf_admission_beta12_2`: PASS on host.
- `test_ee_block_mapping_r1305`: PASS.
- `test_ee_mapped_fetch_r1293`: PASS.
- `test_ee_elf_loader`: PASS.
- Wii cross-compile devkitPPC r50/libogc 3.1.0: PASS.
- Wii MEM1 verifier: PASS; 2,703,468 bytes remaining (min 1,048,576).
- The unrelated `test_ee_aot` could not run through generic `build_host_core.py` because it needs its own fixture include path; this is a test runner setup limitation, **not** a claimed pass.
- Physical Wii/Dolphin boot and FPS on BETA12.2: **NOT TESTED**.

## Next field-run evidence

- Run Crash E3 first, with a separate fresh log and GX off to isolate JIT/GS. Compare `EE_BLOCK.retired`, `EE_NATIVE_WINDOW.retired`, and `EE_SCALAR_HOT` after the old 46-second plateau. Do not interpret increased EE throughput as proof of correct emulation.
- Run GT3 using the same BETA12.2 DOL separately. Report what's visibly displayed and whether buttons navigate any prompts; distinguish from controller-free waiting.
- Confirm both tests' log header `BUILD checkpoint=` / title, because the supplied two logs are from different build configurations (`R1389` versus `R1389-SPEED-BETA12`).
- No private ROM, disc, guest RAM, copyrighted assets or personal runtime log is distributed in the release package.
