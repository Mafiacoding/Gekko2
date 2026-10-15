# Gekko2 SPEED BETA12.14 – Batch 1–5: Entwicklung und Prüfergebnisse

Basis: R1389-SPEED-BETA12.13-GS-ALIAS; der Build erhält die vorhandenen SPEED-/DSI-/GX-Flags und nutzt -O1.

## Batch 1 — GS/GX / Framebuffer
- Native PSM32-/PSM16-/indexierte Adressen werden im 4-MiB-GS-eDRAM-Ring maskiert, einschließlich schneller PSM32-Span-Zugriffe.
- Lokaltransfers, die über das physische VRAM-Ende reichen, erhalten für die Aliasprüfung eine konservative [0,4MiB)-Hülle, damit kein echter Alias übersehen wird.
- Der 16.384-Pixel-Snapshot-Schutz für überlappende Transfers bleibt aktiv. Der konservative Envelope kann bei großen Transfers weiterhin ablehnen; der GX-Renderer deckt noch nicht alle Aliasfälle ab.
- Tests: test_gs_wrap_beta12_14, test_gs_alias_sync_beta12_13, test_gs_local_move_beta12_10, test_gs_indexed_memory.

## Batch 2 — VU1/GIF
- Defensive XGKICK-PATH1-Übernahme: Ein direkter erneuter Ausgabestart kann eine bereits von GIF blockierte Quelle nicht mehr überschreiben; ein Start ist maximal einstufig zurückstellbar.
- Der vorhandene präzise VU-Pair-Stall verhindert im normalen Mikrocode bereits das Retirieren des zweiten XGKICK. Die zweite Stufe ist keine vollständige GIF-Arbitration oder VU-FMAC-Simulation.
- Tests: test_xgkick_timing_aot3 inklusive neuem PATH1-Backpressure-/Stall-Fall.

## Batch 3 — IOP JIT / Scheduler
- IOP-Blockbildung behandelt SYSCALL, BREAK, COP0 (einschließlich RFE/MTC0) und JR/JALR/Branches als präzise Grenzen. Kein künstliches Retire, kein zusätzlicher Opcode behauptet.
- Test: test_iop_block_boundaries_beta12_14.

## Batch 4 — IPU / MPEG
- Häufige inaktive IPU-Serviceaufrufe brechen sofort ab, wenn weder aktive IPU-Befehle noch ein aktiver ToIPU/FromIPU-DMA oder ARM-Worker vorliegen.
- Aktive DMA-STR-Kanäle durchlaufen weiter den bestehenden IPU-Service. NULL-FIFO-Puffer werden nicht dereferenziert.
- Test: test_ipu_idle_beta12_14.
- Nicht implementiert: vollständiges MPEG-2-Decoding, alle Befehlsvarianten, exakte FIFO-Zyklen. Dieser Batch ist ein Scheduler-/Sicherheits-Fix, kein neuer MPEG-Decoder.

## Batch 5 — DMA / Hardware
- Allgemeiner EE-DMA-Kettenmodus ergänzt CALL/RET mit CHCR.ASP und ASR0/ASR1. CALL überträgt zuerst sein Inline-Payload, pusht die korrekte Rücksprungadresse und folgt ADDR; RET überträgt sein Inline-Payload und poppt. Leerer RET-Stack beendet die Kette; Überlauf wird abgewiesen.
- Test: test_dma_call_ret_beta12_14 (zweifach verschachtelt, Payloadreihenfolge, Unterlauf und Überlauf).
- Nicht vollständig: MFIFO-Sonderfälle, REFS-STADR für alle Kanäle, VIF1-Reverse-Chain und vollständiges DMA-Timing.

## Bau- und Teststatus
- devkitPPC 16.1.0 / libogc 3.1.0; Wii-Cross-Build bestanden.
- tools/verify_dol_r1352.py: DOL-/ELF-Codebytes identisch, Abschnittsprüfung bestanden.
- tools/verify_wii_memory.py: freier statischer MEM1-Bereich 2.858.284 Bytes (>1 MiB Mindestwert).
- Binäre Versionskennung ausschließlich R1389-SPEED-BETA12.14-BATCH1-5; BIOS und Disc nicht enthalten.
- Host-Tests: oben aufgezählte Regressionen bestanden. Eine ältere Vollsuite wurde wegen eines bereits im Testtreiber fehlenden libchdr/include-Pfades nicht vollständig ausgeführt; das ist kein Wii-Buildfehler.
- Kein echter Wii-Test. Keine bestätigte Bootzeit-, FPS- oder Gameplay-Verbesserung gegenüber BETA12.13.
- BETA12.14 ist ein Diagnostik-/Integrationsbuild mit -O1; ältere -O3-FPS nicht direkt vergleichen.

## Offene Arbeit / nächste Batches
1. Crash-E3-Frontbuffer/GS DISPLAY-Reconciliation in den Wii-Logs untersuchen; unterschiedliche Farb-/Tiefen-/Textur-Aliase und GX Residency vervollständigen.
2. VU1-FMAC-Timing, XGKICK-PATH-Arbitration mit mehr als einer wartenden Ausgabe, VU0-M-Kontext und GIF/VIF-DMA-Ordering vervollständigen.
3. EE-/IOP-JIT: weitere Sonderinstruktionen, präzise Exceptions, liveness/128-Bit-Register, invalidationssichere Block-Verknüpfung.
4. IPU: hardwaretreue IDEC/BDEC/VLC/IDCT/CSC/PACK-FIFO- und MPEG-2-Implementierung; keine synthetischen Frames.
5. DMA/CDVD/SIF/SPU2/Controller und GT3-Memory-Card-Handshake vervollständigen und pro Spiel testen.

Hardware-Referenzen: PCSX2 GSRendererHW / GSTextureCache (moderne GS-Aliasing-Regeln) und PCSX2 Hw.cpp (EE-DMAC CALL/RET mit ASR-Stack). Source-License: GPLv3; Upstream-Credits beachten.
