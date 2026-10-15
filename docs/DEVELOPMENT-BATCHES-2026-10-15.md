# Entwicklungsfortsetzung: überprüfbare Teilbatches

Ausgangspunkt: main ca16340 / BETA12.20. Main wurde nicht verändert.
Die Branches sind gestapelt; jeden PR gegen den vorherigen Branch prüfen.
Alle Builds sind Diagnoseartefakte mit unverändertem BETA12.20-Release-String,
keine neue Releasefreigabe. DOL-SHA und Batch-Dateinamen unterscheiden sie.

| Batch | Implementierter Schritt | Noch offen |
| --- | --- | --- |
| 1 | PSM-gerechte Pufferdiagnose, Register-/Raster-/Present-/Transport-Snapshots; XFB bleibt bei GPU-Readbackfehler erhalten | tatsächliche Crash-E3-Ursache/Gameplay, EFB-/XFB-Bildbelege; private Wii-Läufe fehlen |
| 2 | opt-in native-only ABI mit über unabhängige Blöcke erhaltenen 128-Bit-Lanes, saubere Helfersicht und vorhandenen Kantenprüfungen | allgemeines Mapping, Dirty-Writeback, native Branch-/Speicher-/opake Körper, vollständige DMA/ASID/Eviction-Oracles und Wii-Test |
| 3 | nachgewiesenen RGB_ONLY/FBA-Schreibmaskenfehler in Software und GX-Import behoben | vollständige TEV-/Blend-/AA1-/Dither-/Feedback-/Alias-Parität |
| 4 | begrenzter SIGNAL-Rückstau, genaue Retry-/DMA-/VU1-Fortsetzung im vorhandenen Konsumentenmodell, SIGLBLID-MMIO | genaue physische Queue-/FIFO-Zyklen, übrige VU-Pipeline und EE-/IOP-Scheduler-Treue |
| 5 | SPR-Interleave-DMA und korrekte Fehlerabschlüsse; unabhängige MPEG-Intra-Proben ergänzt | MFIFO-Interleave, vollständiges IPU/MPEG/CDVD/SIF/Memory-Card/Peripherie-Modell |

Detailberichte und Testkommandos: BATCH1-SCANOUT-DIAGNOSTICS.md,
BATCH2-PERSISTENT-ABI.md, BATCH3-ALPHA-WRITE-MASK.md,
BATCH4-SIGNAL-BACKPRESSURE.md und BATCH5-SPR-INTERLEAVE.md.

Nachweise: 35 aktuelle Host-Suites PASS; 1.800 neue und 2.400 vorhandene
emittierte PPC-Oracles; 105 verlinkte Interpreter-/Native-ABI-Vergleiche
mit 126 wirklich ausgewählten Nachfolgern; 4.096 Alpha-Maskenfälle;
480 SPR-Byteoracles; vier MPEG-Intra-Muster/1.536 Samples (maximal 1 Differenz).
Jeder Batch wurde mit moderner devkitPPC-Toolchain gebaut; DOL/ELF und MEM1
wurden geprüft. Keine reale Wii-/Dolphin-Ausführung, keine FPS-Zusage.

Für Crash-E3-Fortsetzung werden zwei vergleichbare private Kaltstarts
(GX/Software) mit SESSION, GS_DISPLAY_REGS, GS_GRID_HASH, GS_PRESENT,
GS_TRANSPORT, GIF_SIGNAL_STALL, FILEIO_TRACE und EE/IRQ-Zustand benötigt.
Kein erzwungener Backbufferwechsel und keine erfundenen Spieldateien.

Git-Push funktioniert; die GitHub-API ist aktuell schon beim Proxy-CONNECT
mit 403 gesperrt. api.github.com wurde dem Umgebungsentwurf hinzugefügt.
PR-Beschreibungen und Branch-/Basisliste werden mit den Artefakten geliefert;
es wurden noch keine PRs angelegt oder Änderungen nach main gemergt.
