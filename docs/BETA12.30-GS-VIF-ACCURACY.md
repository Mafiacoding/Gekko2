# BETA12.30 – GS/VIF-Accuracy-Batches

Basis BETA12.29 `29cd287`. Hauptbranch bleibt unverändert BETA12.20 `ca16340`.
Fortlaufender Gesamtauftrag: [Batchliste](COMPLETION-BATCHES.md) und
[Einzelkatalog](COMPLETION-CATALOG.md). Dieser Stand schließt einzelne
reproduzierte Lücken; der gesamte Emulator ist weiterhin unvollständig.

- [C01 GS-Register/Interrupts](COMPLETION-GS-IRQ-ACCESS.md): LB/LH/LW/SB/SH/SW
  erreichen GS-CSR/IMR/SIGLBLID; W1C ohne Nebenquittierung; pending IMR-Unmask.
- [C02 Textursampling](COMPLETION-GS-LOD-MTBA.md): Q/K/L, Per-Fragment-LOD,
  sechs MMIN-Modi, trilineare Übergänge, Mip-Koordinaten und MTBA.
- [C02b GX-Filtergates](COMPLETION-GX-FILTER-GATES.md): gemischte Filter bleiben
  im präzisen Rückfall; Source-Shading benutzt interpoliertes Q.
- [C05a VIF-CPU-FIFO](COMPLETION-VIF-CPU-FIFO.md): volle Queue parkt SQ vor
  Retirement, VIF0 acht/VIF1 sechzehn qwords, genau einmalige STC-Wiederaufnahme.
- [C09a SPU2 ENVX](COMPLETION-SPU2-ENVX.md): echte Mixer-Gainänderung bei
  Gastschreibzugriff, erhaltene ADSR-Phase/Zähler und überlaufsicheres PMON.

Keine Native-to-Native-ABI-Aufweichung, keine gestrichenen PPC-Saves und keine
übersprungenen DMA-/TLB-/IRQ-/Budgetgrenzen. Chain, PSM und GX-Risk bleiben
unabhängig und ab Werk OFF. Keine Rückkehr der automatischen 4:3-Verkleinerung.

Gezielte Nachweise: 32768 IMR-Masken-/Ereigniskombinationen, 11701 Mip-/MTBA-
Fälle, 200 VIF-SQ-Retries, 8230 ENVX-/PMON-Fälle und bisherige Soundprüfungen.
Die vorhandene 70-Suite-Baseline bestand nach C01/C02. Die vollständige neue
**78-Suite-Regression besteht**. Neue PPC-Fixtures prüfen auf beiden tatsächlichen
Wii-ELFs je 40 GS-/VIF-/ENVX-Zugriffe und Rasterfälle. Die bisherigen längeren
JIT-/Scheduler-Gates laufen separat und werden nach Abschluss protokolliert.

Reproduktion in frischen Quellen:
```
python3 tools/test_beta12_30_host.py --output-json /tmp/beta12-30-host.json
bash tools/build_beta12_30_accuracy.sh
CHAIN_RESIDENCY=0 bash tools/build_beta12_30_accuracy.sh
```

SDK: devkitPPC r50/GCC16.1.0/libogc3.1.0, offizielles Image
`devkitpro/devkitppc@sha256:4c919aa26151dd43d88ca28c922d1fe2409579a8ba60ef56517baf1abdfb1a48`.
Buildflags entsprechen der bisherigen SPEED/DSI/AOT/GX/Idle/Lightrec-Baseline.
Beide frischen Builds bestehen DOL-/ELF-Sektionsvergleich und statisches MEM1-Gate.
Emulator-Buildcommit `640a761`; nachfolgende Tools/Berichte ändern keine Semantik.

| Variante | DOL-Bytes | MEM1-Ende | Statischer MEM1-Rest |
|---|---:|---|---:|
| Chain kompiliert, Runtime OFF | 2083672 | `0x815d9a94` | 2254188 |
| Ohne Chain | 2078072 | `0x815b8494` | 2390892 |

Statischer Rest ist kein dynamischer MEM2-/Heap-/Stabilitätsnachweis.
Compile-Warnungen, insbesondere ältere Lightning-/libogc-Warnungen, sind
nicht als beseitigt behauptet. Keine Dolphin-/physische-Wii-Ausführung.

Offen: genaue GS-Float-/LOD-/AA-/Feedback-/EXTWRITE-/Field-Semantik und texturierte Punkte/Linien,
GPU-Mip-/gemischte Filterpräzision, VU/VIF-/DMA-/IPU-/Schedulerkonformität,
allgemeines JIT-Dirty-Writeback, Gaussian/Reverb/Silent-Voice-Timing,
weitere Karten-RPCs und Geräte. Echte Wii-Szenen für BIOS-Kacheln, Crash E3,
Tekken/GT3/KOF, Laufzeitstabilität, MEM2-Peaks und Geschwindigkeit bleiben unbelegt.
Ein behobener synthetischer SIGNAL-ACK-Fehler ist kein abgeschlossener Crash-Fix.
