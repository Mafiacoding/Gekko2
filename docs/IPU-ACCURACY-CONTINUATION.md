# IPU/MPEG und DMA-Stall-Control: geprüfte Fortsetzung

Basis: `9571fc131641d46c92d5fa05986f5eea7ee0c724`
(`codex/batch5-spr-interleave`), aufbauend auf allen fünf bisherigen Batches.
GitHub-main wurde erneut als `ca16340e14381cb29a1b108c2553b3f35d71ab40`
geprüft. Arbeitsbranch: `codex/ipu-mpeg-accuracy`; kein Merge nach main.
Der Besitzer verwendet SCPH-50004. In dieser Umgebung liegt weiterhin kein
BIOS-/Spielabbild für einen eigenen OSDSYS-/Crash-/Tekken-Lauf vor.

## Nachgewiesene Fehler und Korrekturen

| Pfad | Vorher | Jetzt |
| --- | --- | --- |
| IDEC-Makroblockheader | Quantisierung vor `dct_type` gelesen; bei DTD+QUANT wurden beide Felder falsch dekodiert | DCT-Typ vor optionalem Quantisierungscode, Header atomar aus Lookahead übernommen |
| IDEC-Anfangsquantisierung | Anfangswert wie BDEC abhängig von QST umgerechnet | Direkter Anfangs-QSC gemäß PCSX2 `ipuIDEC`; BDEC und spätere Bitstrom-Updates behalten ihre Code-Umrechnung |
| IPU_TOP.BUSY | Bei IDEC/BDEC trotz laufender Arbeit null | Bei IDEC/BDEC/VDEC/FDEC gesetzt; CMD.DATA.BUSY bleibt auf VDEC/FDEC beschränkt |
| IDEC-Picture-Type | Alter PCT blieb sichtbar | IDEC setzt PCT auf I-Picture, entsprechend der Referenz |
| DMA FromIPU | STS=fromIPU ließ STADR unverändert | Startadresse beim Start, danach nur tatsächlich übertragene Ausgabe veröffentlicht |
| DMA VIF1/GIF REFS | STD mit MFIFO-Selector verwechselt: 2/3 statt 1/2 | STD=1 VIF1, STD=2 GIF; STD=3 hält diese Kanäle nicht an |

Bestehende FIFO-/Retry-/IRQ-/Write-Notify-Verträge bleiben erhalten. Kein
neues Zustandslayout, kein kosmetischer Pufferwechsel, keine Gast-PC-Patches.
Zusätzlich wurden zwei überholte Testannahmen korrigiert: der R1384-Test
verwendete selbst den falschen GIF-STD-Wert; der R1334-Profiltest erwartete,
dass das seit R1337 implementierte IDEC noch als unimplementiert zählt.
Letzterer prüft jetzt den tatsächlich nicht unterstützten Opcode 15.

## Referenzabgleich

Gelesene PCSX2-Referenzrevision:
`3c8df07e8e367caef8386fd949d6ca44bf2f7ad0`.
Eigenständige kleine Änderungen in vorhandenen, bereits attribuierten Pfaden;
keine Übernahme des PCSX2-CPU-/Scheduler-/SIMD-Backends.

- [IPU.cpp](https://github.com/PCSX2/pcsx2/blob/3c8df07e8e367caef8386fd949d6ca44bf2f7ad0/pcsx2/IPU/IPU.cpp): `ipuIDEC`, `ipuBDEC`, `IPUCMD_WRITE`.
- [IPU.h](https://github.com/PCSX2/pcsx2/blob/3c8df07e8e367caef8386fd949d6ca44bf2f7ad0/pcsx2/IPU/IPU.h): `SetTopBusy`, `SetDataBusy`, Befehlsfelder.
- [IPU_MultiISA.cpp](https://github.com/PCSX2/pcsx2/blob/3c8df07e8e367caef8386fd949d6ca44bf2f7ad0/pcsx2/IPU/IPU_MultiISA.cpp): `GetMacroblockModes`, `mpeg2sliceIDEC`, Worker-Abschluss.
- Lokale Referenzen `docs/reference/pcsx2/pcsx2/Dmac.h` (getrennte STS-/STD-/MFD-Enums) und `IPU/IPUdma.cpp` (`dmaIPU0`, `IPU0dma`).

Das ist ein Abgleich mit einer fixierten Emulatorreferenz und unabhängigen
synthetischen Oracles, kein neuer physischer PS2-Konformitätsnachweis.

## Tatsächlich ausgeführte Prüfungen

`test_ipu_idec_fields`: **384 Fälle, 399.764 Checks, 0 Fehler**.
Vor der Korrektur dieselben synthetischen Eingaben: 45.272 fehlgeschlagene
Checks. Eigenständiger Bitstromgenerator, alle 32 QSC-Werte, lineare und
nichtlineare Updates, DTD an/aus, Frame-/Field-DCT, Anfangsquantisierung,
PCT, TOP/CMD-BUSY einschließlich 64-Bit-MMIO, Reset, Checkpoint im
Ausgabestau, Startcode/TOP und genau ein IPU-IRQ.
Ein Nichtnull-AC-Koeffizient und verschiedene Luma-DC-Prädiktoren machen
Quantisierungs- und Zeilenanordnungsfehler in den wirklichen Ausgabepixeln
sichtbar. Der unabhängige Kosinus-IDCT-Oracle erlaubt maximal zwei RGB-Werte
Rundungsdifferenz nach CSC; Alpha muss exakt stimmen. QSC=0 ist ein gezielter
Register-/Decoder-Randfall, keine Behauptung eines regulären MPEG-Streams.
Zusätzlicher ASan-/UBSan-Build dieses Tests: dieselben Checks, 0 Fehler
(`ASAN_OPTIONS=detect_leaks=0`).

`test_dma_ipu_stall`: **748 Checks, 0 Fehler**; ursprünglicher DMA-Code mit
demselben endgültigen Test: 23 fehlgeschlagene Checks. Vier STS-Einstellungen,
echte IPU-Ausgabe-FIFO, leere Ausgabe, partielle Fortschritte, RAM-Bytes,
JIT-Schreibbenachrichtigung, QWC/STR/Completion; jeweils vier STD-Einstellungen
mit echten VIF1-/GIF-Konsumenten und partiellem REFS-Retry. Der leere END-Tag
lädt abschließend seine Inline-Adresse in MADR, auch ohne Payload.

Aktueller Host-Regressionslauf: **14 ausführbare Suites, 14 PASS**:
beide neuen Tests, `test_hardware_accuracy_r1384`, BDEC/IDEC R1337,
VLC/PACK R1336, IPU-Profil R1334, SPR-Interleave, GIF-SIGNAL-Rückstau,
GS-Kontextoffset, Dreieckskanten, AFAIL/FBA, IPU-R1335 und IPU-Idle.
Die letzten beiden benötigen ihre separaten Linkbefehle (R1335 zusätzlich
`arm/worker.c`; Idle nur `ipu.c` und `runtime_profile.c` neben dem Test).
`build_host_core.py` baut Testprogramme; sie müssen danach ausgeführt werden.

Unabhängiger FFmpeg-7.1.5-Abgleich der bestehenden BDEC-Ausgabe erneut PASS:
vier eigene MPEG2-Intra-Muster, 1.536 YUV-Samples; maximal ein Samplewert
Abweichung, Grau exakt. Das erweitert nicht die P-/B-Frame-Abdeckung.

Verlinkter Wii-PPC-Code in Unicorn:

```text
PASS linked PPC IDEC: 36 cosine-oracle macroblocks, quant/DCT headers, TOP busy/reset, PCT, FIFO output
PASS linked PPC FromIPU CSC DMA: start/progress STADR, output bytes, QWC/STR and completion
PASS linked PPC REFS: all 4 STD selectors for VIF1/GIF, partial progress and completion
PASS linked PPC VDEC tables/signed data/TOP starvation and PACK actual 64->32-QWC DMA RGB16
```

## Reproduktion und Wii-Build

```sh
python3 tools/build_host_core.py test_ipu_idec_fields test_dma_ipu_stall
validation/r1386-host/bec8fbc68a0adfff/test_ipu_idec_fields
validation/r1386-host/bec8fbc68a0adfff/test_dma_ipu_stall
bash tools/build_ipu_accuracy.sh
python3 tools/verify_ipu_fields_linked.py Gekko2-BETA12.20-IPU-Accuracy.elf --nm "$DEVKITPPC/bin/powerpc-eabi-nm"
python3 tools/verify_ipu_video_r1336.py Gekko2-BETA12.20-IPU-Accuracy.elf --nm "$DEVKITPPC/bin/powerpc-eabi-nm"
```

Der ausgegebene Host-Cachepfad hängt von Compilerflags ab. PPC-Prüfungen
benötigen Unicorn 2.1.4; Plattformdienste werden modelliert, IPU/DMA-Code
wird aus dem tatsächlichen ELF ausgeführt.

Build mit devkitPPC r50/GCC 16.1.0/libogc 3.1.0, offiziellem Image
`devkitpro/devkitppc@sha256:4c919aa26151dd43d88ca28c922d1fe2409579a8ba60ef56517baf1abdfb1a48`.
Frisches `build-ipu-accuracy`, `-O1`, sämtliche bisherigen SPEED/DSI/AOT/GX/
ARM/IDLE/LIGHTREC-Buildflags beibehalten. Experimentelle Chain-Residenz bleibt
als gesondertes Opt-in im Code erhalten und in diesem Diagnosebuild AUS.
Vorhandene Compiler-/Vendorwarnungen bleiben sichtbar; kein warnungsfreier
Build behauptet. Release-String bleibt BETA12.20, keine Releasefreigabe.

DOL-/ELF-Sektionsabgleich PASS; statisch **2.847.692 Bytes MEM1 frei**.
DOL-SHA256: `a4f3f46f2b0543ade6dbe4c8c9db3d93392479a49bd544e7e64113914a1dda68`.
Keine Dolphin-/Wii-Ausführung, keine gemessene FPS-Verbesserung.

## Weiter offen

Die Fehler sind für reale MPEG-/DMA-Befehlsfolgen relevant; sie beweisen
keine Ursache oder Behebung der SCPH-50004-Kacheln, von Crash E3 oder Tekken.
Für diese drei Fälle fehlen hier aktuelle private Lauf-/Bildbelege.

Priorisierte Fortsetzung:

1. SCPH-50004-/Crash-Ausgabe am gleichen Gastzustand zwischen Software und GX
   vergleichen: Primitive/Vertexdaten, Ziel-/Displaypuffer, GIF/VU und Present.
2. Breitere MPEG1-Escape-/Alternate-Scan-/Nicht-Intra-/Fehlerstrom-Oracles,
   zeitliche IPU-/DMA-Fences und Gast-Motion-Compensation-Integration prüfen.
   IPU-BDEC allein ist kein vollständiger Filmplayer oder P-/B-Frame-Decoder.
3. Weitere native EE-Branch-/Speicherpfade, dynamische Registerzuordnung und
   Dirty-Writeback ergänzen, bevor Registerresidenz allgemein aktiviert wird.
4. GS/GX-Blend-/Dither-/AA1-/Feedback-/Alias-Parität und Textur-Wiederverwendung;
   danach anhand Wii-Messungen weitere nullDC4Wii-Prinzipien anwenden.
5. VU-/Scheduler-/FIFO-Zyklentreue, weitere DMA-/CDVD-/SIF-/Memory-Card-/SPU2-
   Protokolle. FromIPU-Destination-Chain ist nicht ungeprüft zu erfinden:
   die gelesene PCSX2-Referenz verlangt für diesen Kanal Normalmodus.

Die vorherige Gesamtübersicht bleibt in
[DEVELOPMENT-BATCHES-2026-10-15.md](DEVELOPMENT-BATCHES-2026-10-15.md).
"Alles implementiert" oder Wii-Spielbarkeit wird nicht behauptet.
