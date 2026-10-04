# R1256 — Wii frontend, safe cold boot and scratchpad DMA

## Benutzung
Der neue native Startbildschirm erscheint vor dem BIOS-Boot. Dunkelblauer
Hintergrund, blaue geometrische Linien, cyanfarbene Auswahl und eigenes
Bitmap-Rendering. Keine Console-Textueberlagerung im Startmenue.
GameCube-Controller: Steuerkreuz waehlt, A oeffnet/startet, B verlaesst Untermenues.
BIOS / OSDSYS startet ohne Disc; START DISC sucht game.bin, dann game.iso unter
sd:/pcsx2/games/. Die bisherigen BIOS-Suchpfade bleiben erhalten, SCPH50004.bin
hat Vorrang. A startet einen neuen Coldboot und gibt vorher beide CPU-RAMs und
Disc-Handles frei; START setzt die pausierte Sitzung fort. B+Z pausiert den Boot
und kehrt unmittelbar zum Startbildschirm zurueck.
Settings: A schaltet den Diagnose-HUD um, X waehlt 50000/10000 IOP-Slices pro
Bildschirmausgabe. Durchsatz vs. Reaktionszeit ist ein Budget, kein FPS-Limit.
Interpreter/JIT wird durch die ELF ausgewaehlt, kein vorgetaeuschter Laufzeitschalter.

## Korrekturen
- Vollstaendigen gesicherten R1255-Quellstand wiederhergestellt, bevor das
  Frontend uebertragen wurde. Alle dortigen RPC/PAD/GS/BC0-Korrekturen enthalten.
- Budgetzaehler folgt dem wirklich ausgewaehlten Chunk; Init-Fehler werden behandelt.
- FAST-Builds schreiben nach normalen Scheduler-Budgetgrenzen keine drei
  Diagnosezeilen mehr in die Konsole. Halt-/Fehlerdiagnosen bleiben erhalten.
  Das spart Ausgabe und verhindert solche Console-Schreibzugriffe in GS-Bilder.
- Beobachteter Tekken-DMA-Kanal9 hatte keinen Sink und verwarf Daten. toSPR
  kopiert jetzt Nutzdaten aus EE-RAM ins Scratchpad, fromSPR-Nutzdaten umgekehrt;
  SADR laeuft bei 16KiB um. RAM-Grenzen werden vor dem Kopieren geprueft.
  toSPR-Source-Chain-Nutzdaten verwenden denselben Kopierpfad.
  MFIFO-Produzent/Drain-Timing, vollstaendige fromSPR-Destination-Chains,
  Interleave und SPR-Tagtransfer sind damit weiterhin NICHT vollstaendig modelliert.

## Verifiziert
- Beide devkitPPC-ELF/DOL-Builds erstellt (Interpreter und bestehender PPC-JIT).
- 158/158 native Regressionstests, inklusive neuer SPR-Daten-/Grenztests.
- Tatsaechlicher Big-Endian-PPC-Code aus der ELF: 53 GS + 19 CLUT + 6 SPR-Pruefungen.
- Drei BIOS-Core-Init/Boot/Shutdown/Cold-Reset-Zyklen erfolgreich.
- Neues Frontend mit demselben Renderer hostseitig als Vorschau gerendert.
  Kein kompletter Dolphin-/Wii-Test der Benutzeroberflaeche in dieser Umgebung.
- Tekken-BIN aus allen drei Split-ZIP-Teilen mit CRC-Pruefung wiederhergestellt.
  SLUS_200.01, NTSC, 728143920 Bytes. Nach SPR-Fix frischer 160M-EE-Boot ohne Halt,
  REND71, GIF1454 QW/563 Sprites, aktive Job-Warteschleife, weiterhin schwarzes Bild.
- Diskless nach SPR-Fix: 600M-EE-Coldboot ohne Halt, echte Sony-Startanimation
  sichtbar. Noch kein Browser/System-Configuration-Menue.

## Offen
Das native Frontend ist fertig; das emulierte OSDSYS-Menue ist nicht erreicht.
Kein Namco-Logo und kein bestaetigter FPS-Gewinn. JIT nutzt den vorhandenen
Recompiler; keine neue JIT-Optimierung behaupten. Die fehlenden SPR-/MFIFO-
Details anhand echter Transfers verfolgen, nicht Gast-Jobzaehler erzwingen.
Build: tools/build_r1256.sh. Frontend-Vorschau: tools/preview_frontend.c.
Font: DejaVu Sans, Lizenz docs/licenses/DejaVu.txt.
Checkpoint enthaelt Quellcode/ELF/DOL/Patch/Testnachweise, keine BIOS-/Disc-Daten
und keine RAM-Savestates. Aenderungen gegen das urspruengliche Claude-Archiv
liegen als .patch und .diff vor.
