# Gekko2 BETA12.11 — GS Pixel-Format-Transfers (15 October 2026)

## Umfang
BETA12.10 ist die Ausgangsversion; BETA12.11 erweitert `gs_mem_copy_local()`.
Ein GIF `TRXDIR=2` bleibt ein sofort ausgeführter lokaler GS-VRAM-Transfer.

### Aktive, hardwareorientierte Transfers
Unterstützte Source- und Destination-PSMs: PSMCT32 (0x00), PSMCT24 (0x01),
PSMCT16 (0x02), PSMCT16S (0x0A), PSMT8 (0x13), PSMT4 (0x14),
PSMT8H (0x1B), PSMT4HL (0x24), PSMT4HH (0x2C),
PSMZ32 (0x30), PSMZ24 (0x31), PSMZ16 (0x32), PSMZ16S (0x3A).

Transfers zwischen verschiedenen PSMs mit gleicher Transferbreite kopieren die
rohen Pixelwerte entlang der jeweiligen Swizzle-Adressierung. Hierzu gehören
CT16↔CT16S, CT32↔Z32, CT24↔Z24, CT16↔Z16 sowie indexed 8↔8H und 4↔4HL/HH.
Die PSMCT24/PSMZ24-High-Bytes bleiben unberührt. RGB und Z werden **nicht**
in Interpretation oder Skala umgerechnet, sondern mit passender Bitbreite kopiert.

### Experimentelle, ausschließlich auf Opt-in beruhende Softwarekonvertierungen
`gs_mem_set_local_extended_conversion(1)` ist eine programminterne API;
die Wii-Version schaltet sie derzeit **nicht** ein. Implementierte Fälle:
RGB 16↔32/24 per 5-5-5-1 ↔ 8-8-8 + PS2 Alpha (0 oder 0x80),
RGB 24↔32 mit definierter Alpha-Behandlung, indizierte 4↔8 und Z-Tiefen
16↔24↔32 durch Trunkierung/Erweiterung. Das ist kein bestätigtes
Hardwareverhalten der PS2 und darf nicht pauschal in GIF aktiviert werden.

### Absicherungen
Vollständiger GPU-Readback vor CPU-Moves, VRAM-Page-Bounds vor Schreibbeginn,
per-PSM Swizzle-Geometrie, überlappungssicherer Same-Layout-Copy.
Physisch überlappende, verschiedene Swizzle-Layouts werden bewusst abgelehnt.
Die vorherige BETA12.10-Überlappungssicherung bleibt erhalten.
Die Logzeile `GS_PSM_CONVERT converted_pixels=... rejected_formats=... extended=...`
zeigt den tatsächlichen Einsatz.

## Verifikation
PASS: neuer `tests/test_gs_psm_convert_beta12_11.c` (native 16/16S,
CT/Z32, CT24 Alpha, PSMT8/8H, strikter 32/16 Reject,
optionales 32↔16/24, Alias-Reject, 8→4).
PASS: BETA12.10-Lokalkopien inklusive 16S, 8, überlappender Rechtecke,
GX-Readback-Fehler.
PASS: GIF A+D Integration von `BITBLTBUF`, `TRXPOS`, `TRXREG`, `TRXDIR=2`.
PASS: Wii devkitPPC 16.1.0 Cross-Build, ELF↔DOL-Sektionen, statische MEM1-Grenze.
BETA12.11 ist NICHT auf echter Wii getestet; FPS oder korrektes Spielbild
lassen sich aus Host-Tests nicht ableiten.

## Abgleich / Quelle
PCSX2 `pcsx2/GS/GSState.cpp`, `GSState::Move()`:
https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS/GSState.cpp
PCSX2 PSM-Speicherlayout-Diskussion:
https://github.com/PCSX2/pcsx2/discussions/4313
Play! Referenzprojekt (alternativer PS2-Emulator):
https://github.com/jpd002/Play-
Die Erweiterung ist eigenständig entwickelt; keine Zusage, dass Play!/PCSX2
beliebige Farbkonvertierung in Hardware-GS-Moves durchführen.

## Bestandsaufnahme weiterhin offener Module — KEIN Fertigstatus
1. GIF `TRXDIR=1` Local-to-Host: Hostdatenstrom bei der GIF-Übertragung
   unvollständig (`source/hw/gif.c`). Hohe Priorität für VRAM-Readbacks.
2. IPU MPEG-Decoder-/PACK-Ausgabepfad: unvollständig (`source/hw/ipu.c`,
   Status R1335). Für FMV-lastige Titel besonders relevant.
3. VU0/VU1: vollständige FMAC-Flag-Timing-/Pipeline-Treue sowie
   vollständige JIT-Abdeckung; XGKICK/GIF-Arbitration prüfen.
4. GS: physische Aliase zwischen unterschiedlichen PSMs und weitere
   Textur-/Framebuffer-Cache-Invalidierungen bleiben ungelöst.
5. EE/IOP: vollständige Dynarec-Abdeckung, EE↔IOP Scheduler-Timing
   und CDVDFSV-RPC-Kompatibilität bleiben offene Baustellen.
6. GX: komplexe Blend/Depth/Alpha/Texture-Pfade brauchen weitere
   Wii-Hardwaretests; ein schwarzer Frontbuffer beweist keine PSM-Ursache.
7. Play! HLE-BIOS ist ein grundlegend anderer Ansatz als Gekko2s
   BIOS-Boot; ein 1:1-Modulport würde keine geprüfte Kompatibilität garantieren.

Keines dieser sieben Module wurde mit BETA12.11 als vollständig gelöst erklärt.
Für Crash, Tekken und GT3 zuerst Hardwarelogs auswerten; dabei
`GS_PSM_CONVERT`, `GS_LOCAL_COPY`, `GS_BUFFER_COMPARE`, `EE_VBLANK_POLL`
vergleichen. Wiederherstellungs-Checkpoint bleibt BETA12.10.
