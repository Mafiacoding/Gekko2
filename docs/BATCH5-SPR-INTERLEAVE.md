# Batch 5: SPR-Interleave-DMA und unabhängige MPEG-Proben

Basis: gestapelter Batch-4-Branch. Neue Hardwarefunktion ist der bisher
abgewiesene SPR-Interleave-Modus; MPEG/CDVD werden zusätzlich regressionsgeprüft.

## Implementiert

ToSPR/FromSPR MOD=2 verwenden D_SQWC.TQWC für Transferblöcke und SQWC für
übersprungene Quadwords im Hauptram. TQWC=0 überträgt den gesamten Rest in
einem Block. SADR läuft nur über tatsächliche Daten, mit 16-KiB-Wrap. MADR
enthält auch den abschließenden Skip; FromSPR mit STS=fromSPR aktualisiert
STADR. Tatsächliche RAM-Schreibzugriffe durchlaufen den vorhandenen
JIT-Write-Notify. Ein partieller Fehler erhält den tatsächlichen Fortschritt
und meldet keinen erfolgreichen DMA-Abschluss. Auch normale abgewiesene
DMA-Transfers erzeugen jetzt keinen falschen Completion-Status.

FromSPR-Interleave zusammen mit MFIFO wird ausdrücklich abgewiesen, statt
eine nicht abgesicherte Ring-/Producer-Semantik zu behaupten. Normales MFIFO
bleibt unverändert. Exaktes DMA-Timing und Busfehler-IRQ-Modell bleiben offen.

Verhaltensabgleich, eigenständige Bytekopier-/Registerimplementierung:
https://github.com/PCSX2/pcsx2/blob/master/pcsx2/SPR.cpp
D_SQWC-Layout zusätzlich gegen die lokale Dmac.h-Referenz geprüft.

## Tatsächliche Tests

- test_dma_spr_interleave: **480 bytegenaue Oraclefälle PASS**, beide Richtungen,
  QWC=0/1/2/3/7/17, TQWC=0/1/2/5/255, SQWC=0/1/3/255, Scratchpad-Wrap,
  vollständige RAM-/SPR-Vergleiche, ausgelassene Bereiche, Notify-Adressen,
  STADR, Teilfehler, normale Fehler ohne Completion und MFIFO-Reject.
- SPR normal/chain/wrap, MFIFO producer/wakeup, DMA CALL/RET und
  VIF1-GS-Readback-Regressionen PASS.
- IPU bitstream/FIFO/DMA/CSC/ARM-ABI, VLC/PACK, BDEC, IDEC und Idle-Suites PASS.
- IOP-CDVD, ISO-Lader und CDVD-Config-Suites PASS.

Einige ältere Whitebox-Tests benötigen ihre vorgesehenen Einzel-Linkbefehle:
CALL/RET allein aus dem Test-C (enthält dma.c und Transportmocks),
VIF1-Readback-Test mit dma.c, IPU-Idle mit ipu.c/runtime_profile.c.
IPU-R1335 benötigt zusätzlich arm/worker.c. Die anfänglichen generischen
Runner-Linkfehler wurden so aufgelöst; sie sind keine Anwendungstestfehler.

Unabhängige MPEG-Probe: vier selbst erzeugte 16x16-YUV420P-Intra-Bilder,
mit FFmpeg 7.1.5 zu MPEG2 kodiert und unabhängig dekodiert; Gekko2 BDEC
liest den identischen Bitstrom über seinen vorhandenen Referenzdriver.
Reproduktion:

```sh
python3 tools/build_host_core.py test_ipu_mpeg_reference_r1337
python3 tools/verify_mpeg_intra_oracle.py --driver validation/r1386-host/bec8fbc68a0adfff/test_ipu_mpeg_reference_r1337
```

Ergebnis bei je 384 Samples:

| Muster | Abweichende Samples | Maximaler Unterschied |
| --- | ---: | ---: |
| Grau | 0 | 0 |
| Luma-Rampe | 7 | 1 |
| Schachbrett | 8 | 1 |
| Chroma-Rampe | 3 | 1 |

Die Prüfung akzeptiert höchstens einen Samplewert Unterschied durch
unterschiedliche Integer-IDCT-Rundung. Das sind **keine bitgleichen PS2-
Hardwarebelege**, keine P-/B-Frame-Abdeckung und keine vollständige
MPEG-Implementierung. Private BIOS-/Spieldaten werden nicht verwendet.

Zusammengeführter aktueller Host-Regressionslauf: **35 ausführbare Suites,
35 PASS**, einschließlich aller neuen Batches und bestehender GS/JIT/VU/
DMA/IPU/CDVD-Kontrollen. Ausgabe im ausgelieferten host-test-results.json.

Wii-Build: tools/build_batch5_spr.sh, frisches build-batch5-spr;
devkitPPC r50/GCC16.1.0/libogc3.1.0, -O1 und bisherige SPEED/DSI/AOT/GX/IDLE/
LIGHTREC-Flags erhalten; experimentelle Chain-Residenz AUS.
ELF/DOL-Abgleich PASS; statisch **2.848.076 Bytes MEM1** frei.
DOL SHA-256: `33c1fe9b2f9d863aa407a31a6f1a4a651de10ef554ab50fd2bc4eaea38e63e17`.
Keine Wii-/Dolphin-Ausführung und keine reale FPS-/Gameplaybehauptung.

Weiter offen: MFIFO-Interleave, Reverse-Chain-Sonderfälle, vollständige
IPU/MPEG- und FIFO-/Buszyklentreue, CDVD/SIF-Fehler-/Timingfälle, GT3-
Memory-Card-Protokoll sowie weitere Hardware-/Peripheriefunktionen.
