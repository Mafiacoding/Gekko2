# R1275 EE scalar dispatch checkpoint

R1274 source restored from the durable public checkpoint before editing. The CPU loop now bypasses single-instruction JIT dispatch for LUI, ORI, ANDI, XORI, ADDU and SLL, in addition to ADDIU. The native translators remain implemented and callable; VU0/VU1 blocks and IOP JIT remain enabled in the JIT build. No guest boot shortcuts, skipped retirement/events or timing changes were introduced.

## Controlled comparison

Actual Wii ELF execution on a big-endian PPC CPU model. Eight instructions per sample, cold and three warm samples. Allocation and cache-maintenance platform calls mocked, so these are instruction counts, not cycles or FPS. Register results, Count advancement by eight, PC advancement and zero register checked. No new physical Wii or Dolphin boot measurement is available. The user's 0.35 FPS cannot be converted into a promised FPS gain from these counts.

| Instruction | R1274 cold | R1275 cold | R1274 warm | R1275 warm | Warm reduction |
|---|---:|---:|---:|---:|---:|
| ADDIU | 5073 | 5017 | 5073 | 5017 | 1.10% |
| LUI | 6010 | 5129 | 5569 | 5129 | 7.90% |
| ORI | 6031 | 5145 | 5577 | 5145 | 7.75% |
| ANDI | 6028 | 5145 | 5577 | 5145 | 7.75% |
| XORI | 6030 | 5145 | 5577 | 5145 | 7.75% |
| ADDU | 6237 | 5129 | 5585 | 5129 | 8.16% |
| SLL | 6284 | 5156 | 5585 | 5113 | 8.45% |

Native regression suite: 180/180 passed. Both final ELF builds passed inherited PPC CPU/cache/IRQ/GS/config/IOP/VU native tests, including VU0 block/TPC/wrap/branch/fallback and real VIF0 MSCAL/MSCNT integration. First link found a stale zero-byte PPC compiler object; it was removed and rebuilt successfully. Both ELF and DOL are included.

The OSDSYS rendering from R1274 is preserved; this round did not rerun the authentic BIOS menu checkpoint or a fresh cold boot. Full EE multi-instruction block execution with correct per-instruction events remains open, as do the documented VU floating-point and pipeline limitations. This is a measured incremental dispatch fix, not completion of the full JIT port.

Build: tools/build_r1275.sh, devkitPPC/libogc, serial -j1, separate Interpreter/JIT directories. Runtime logs now identify R1275. For hardware comparison use the same BIOS, persistent BIOS config, SD card and diskless settings; compare elapsed boot time and EE_per_s in the PERF log, not only displayed presents/FPS.

Public archive contains no BIOS, game image or guest RAM state. R1274 private menu state remains the prior saved recovery point.
