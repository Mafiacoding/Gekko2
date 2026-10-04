# R1263: independent display clock and modern mcOpen

## Implemented

EE display/VBLANK and GS VSYNC phase now use an independent uint64 core clock.
Guest MTC0 Count, Count reset and 32-bit wrap cannot reposition these events.
The clock advances at the existing approximate instruction/parked timing hooks;
this is not a cycle-accurate scheduler or a PAL timing correction.
Optional ECLK checkpoint tail stores eight explicit little-endian bytes.
Missing legacy ECLK starts at zero, without inheriting live state. Duplicate
and malformed ECLK blocks are rejected. EES1/EINT sizes are unchanged.

Modern MCSERV mcOpen RPC function 2 now returns -4 (missing entry), matching
the existing legacy function 0x71. The cardless HLE has no valid descriptor 0
to return. Reply bounds and modern CLOSE function 3 are tested. Other card
operations retain their existing partial HLE behavior.

## Validation

- Native regression suite: 170 / 170 PASS.
- Actual PowerPC ELF executed under Unicorn: 149 checks pass for each
  Interpreter and JIT build. The new checks cover independent-clock behavior
  and real SifSetDma MCSERV dispatch. These are shared interpreter/core tests,
  not evidence that generated EE/IOP/VU JIT blocks or physical Wii work.
- Both Wii ELF and DOL variants build successfully.
- Complete fresh coldboot from the supplied SCPH-50004 BIOS, with all current
  production changes, reaches Browser / System Configuration using emulated
  CROSS press/release only. Final EE=1,484,740,681, PC=0x0022e2bc, HALT=0.
  Active GS display BP=0, BW=640, source region 640x256 scaled to 640x448.
  See the menu PNG and fresh boot/display logs. The orb remains fragmented.
- The private execution checkpoint contains firmware-derived RAM and is
  excluded. The downloadable checkpoint ZIP contains source, builds and logs,
  with no BIOS, game image, IRX payload or guest RAM.

## Browser Back: unresolved, concrete next investigation

A real controller Browser/Back continuation remains black while both cores
continue executing. Final EE=1,605,514,972, PC=0x00271754, HALT=0.
Clock-only continuation of the older black state for another 500M retired
instructions did not recover it. Normal FRAME/DISPFB double-buffer alternation
was observed; an isolated opposite draw/display buffer snapshot is not proof
of a missing flip. No TRXDIR=2 request was observed in the earlier trace.

Read-only trace of the supplied OSDSYS shows mcGetInfo returns -100 because
its libmc client.server is null; mcSync then returns -1 without an active RPC.
The real mcInit checks its returned MCSERV version >=0x205 and MCMAN >=0x206;
the current INIT HLE returns zero versions, so the guest clears client.server.
No service calls occur during the traced Browser probe sequence.

A fresh early-boot trace confirms INIT function 0xfe at EE=49,969,578 with
both tracked versions zero. LOADFILE path/module metadata tracking runs later,
for ATAD, XFLASH, XFROMMAN and HDDLOAD, not the MC modules. The restored IOP
boot-loader list has 29 entries and no MCMAN/MCSERV. The supplied ROM contains
modern XMCSERV (module name mcserv, version 0x208) and XMCMAN (mcman_cex,
version 0x209), plus legacy MCMAN/MCSERV 0x101. These facts do NOT justify
selecting the highest ROM version globally or forcing client.server.
Next: trace the actual requested IOP restart/boot configuration and module
loading route; report metadata from the provider actually selected/loaded.
The name matcher currently recognizes mcman/mcman_tool but not mcman_cex;
fix that together with verified provider selection and regression coverage.

## Interpretation limits and next work

Earlier VEND -> VSTART gaps of roughly 500 retired instructions do not mean
500 timing ticks: parked time advances timing without retirement. The trace
showed roughly 4.5 million timing ticks across such a gap. Do not cite that
observation as a shortened-frame root cause.

OSDSYS still needs Browser Back, correct orb rendering and persistent BIOS
configuration/NVRAM. The NVRAM ABI investigation is documented separately.
The full EE/IOP/VU0/VU1 JIT port remains unfinished. There is no physical Wii
or Dolphin FPS measurement for this build and no substantiated 10 FPS claim.
Tekken remains deferred until OSDSYS correctness. Historical HLE/boot
compatibility paths remain; this is not a claim of an entirely hack-free core.

## Reproduction

Use tools/build_r1263.sh with devkitPPC, libogc and the compatibility libraries.
Native: tools/verify_regressions.py.
PowerPC: tools/verify_ppc_clock_r1263.py ELF --nm powerpc-eabi-nm.
Controller-only drivers: tools/verify_osdsys_setup_r1263.c and
 tools/verify_osdsys_browser_r1263.c. Supply a user-owned BIOS locally.
Patch and diff are identical cumulative unified diffs against the original
pcsx2-wii-full-debug(1).zip archive; exact apply/byte comparison is checked.
