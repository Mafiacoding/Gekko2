# Fortlaufende Completion-Batches ab BETA12.29

Basis `29cd287`, vollständige Einzel-IDs und Gates in COMPLETION-CATALOG.json.
Jeder Batch bekommt eigene Code-/Regressionstests, Commit/Branch und PR.
Die Reihenfolge ist ein Arbeitsauftrag; ein noch nicht ausgeführter Batch
ist kein fertiger Emulator. Hardware-Gates werden nicht durch Hosttests ersetzt.

| Batch | IDs | Konkrete Arbeit / Abnahme |
|---|---|---|
| C01 GS-Interruptzugriff | GIF-01/02, TIME-02 | EE-Zugriffsbreiten für CSR/IMR, reale W1C-Quittierung, pending-event unmask, alle Producer-Retries |
| C02 Textursampling | GS-02/03, GX-01/04 | Q-basierter LOD pro Fragment, K/L, MTBA-Layout, Mipmapkoordinaten/Filter, Mip-Cache-/GX-Sicherheitsgrenzen |
| C03 Pixelpipeline | GS-04/05/06, GX-02/03/05 | unabhängige ALPHA/ATE/DATE/Masken/Z-/AA-/Feedback-Oracles und belegte GX-Zulassung |
| C04 PCRTC | GS-07/08/09 | EXTDATA/EXTBUF/EXTWRITE, Interlace/Field und Readback/VRAM-Ownership |
| C05 VU/VIF/GIF | VU-01..05, GIF-02, VIF-01/02 | getaktete Flags/VI/Q/P, Pending-XGKICK, FIFO/UNPACK/MPG-/Pfadmatrix |
| C06 DMA/Scheduler | DMA-01/02, TIME-01/02 | Reverse Chain, MFIFO/Interleave/REFS, echte Fristen und keine verlorenen IRQs |
| C07 MPEG/IPU | IPU-01..05 | unabhängige legale Streams, Escape/Field/Scan, IDCT/CSC/PACK und FIFO-/BUSY-Zeit |
| C08 EE/IOP/JIT | EE-04..06, JIT-01..08, IOP-01/02 | FPU-/Cachemodelle, reale Zulassung und dirty Registerübergabe mit EABI/Mapping/SMC-/IRQ-Gates |
| C09 Sound | SPU-04..07 | Gaussian, Reverb-RAM, ENVX/Key-On/Silent-Voice-Timing, DMA/IRQ/Checkpoint |
| C10 Karten | MC-01..08 | Raw Pages, Erase/CheckBlock, Directory-FDs, Port/Slot, Geometrie, in-flight DMA, MagicGate |
| C11 IOP-Geräte/RPC | RPC-01..04, CDVD-01/02, PAD-01 | echte Modul-/Datei-/DMA-Zustände, CDVD-Kommandos, Controller-Modi/Pressure/Vibration |
| C12 Optionale Geräte | IOP-03, DEV-01/02 | korrekte Abwesenheit erhalten; echte GTE/DEV9/USB/i.Link-Geräte nur mit spezifizierter Implementierung und Tests |
| C13 Speicher/Integration | MEM-01/02, PERF-01 | versionierte Checkpoints, Allocationfehler, statische Grenzen und dynamische Wii-Peaks/identische Szenen |
| C14 Spiele-Abnahme | GAME-01..05, GIF-01, GX-05, JIT-08 | echte Wii: SCPH-50004, Crash E3, Tekken, GT3, KOF; Bild, Eingaben, Fortschritt und Gesamtzeit belegen |

C01 ist derzeit in Umsetzung; weitere Batches werden danach direkt bearbeitet.
Keine private BIOS-/Disc-/Karten-/Logdatei wird als öffentliche Fixture verwendet.
