# BETA12.31 / Wii Limits 1.1 – geprüfte Downloads

- [Gekko2-Wii-Limits-1.1-SD.zip](https://raw.githubusercontent.com/Mafiacoding/Gekko2/8a4cd1455387ed1178f51ecbc9af853ba61e33be/downloads/Gekko2-Wii-Limits-1.1-SD.zip) – 245750 Bytes;
  SHA-256 `4f1a89c2d671b6ec114b367c90a4f488139619e8c873571fbc738bf5b1604c57`.
- [Gekko2-BETA12.31-SD.zip](https://raw.githubusercontent.com/Mafiacoding/Gekko2/8a4cd1455387ed1178f51ecbc9af853ba61e33be/downloads/Gekko2-BETA12.31-SD.zip) – 1163642 Bytes;
  SHA-256 `bf1a8f29f5c99116818e922b11277089cbcceaecf98abeb518649660afaa1a67`.
- [Gekko2-BETA12.31-review.zip](https://raw.githubusercontent.com/Mafiacoding/Gekko2/8a4cd1455387ed1178f51ecbc9af853ba61e33be/downloads/Gekko2-BETA12.31-review.zip) – 18687277 Bytes;
  SHA-256 `5d09f06bb23b439e03b28271f392980cd1250997098417d6693bcd5faccc2e1a`.

Artefaktcommit `8a4cd1455387ed1178f51ecbc9af853ba61e33be`, gepackter Quellcommit
`101c245f975beb4300e1a9c06ab38143bad055a4`, Emulator-/Benchmark-Buildcommit `ee6306d`.
Alle drei öffentlichen URLs erneut heruntergeladen: HTTP 200, bytegleich mit
den lokalen Paketen, SHA-256 und vollständige ZIP-CRC bestanden.

BETA12.31-SD enthält beide Apps: `apps/Gekko2/boot.dol` und
`apps/gekko2-wii-limits/boot.dol`, jeweils Metadaten/Icon. Das kleine separate
Benchmark-SD-ZIP enthält nur Wii Limits 1.1. Review-ZIP enthält beide Emulator-
DOLs/ELFs, Benchmark-DOL/ELF, getrackte Quellen und synthetische Nachweise.
Keine BIOS-/Spieldateien, Besitzerlogs/CSV, RAM-/Bild-/Disassembly-Dumps oder
Benutzerkarten/Checkpoints/Profile. Bestehende Einstellungen bleiben erhalten;
Experimente ab Werk OFF.

83 Host-Suites, alle 16 Emulator-PPC-Gruppen und zwei Wii-Cross-Builds samt
DOL-/ELF-/MEM1-Prüfung bestanden. Benchmark 87 Host- und 135 ELF-PPC-Prüfungen.
Keine physische Wii-/Dolphin-Ausführung der neuen Builds, kein belegter FPS-
oder BIOS-Kachel-Fix. [Bericht und Reproduktion](BETA12.31-RUNTIME-PROFILE.md).

Für das Laufzeitprofil dieselbe Szene mit unveränderten Optionen einige Minuten
laufen lassen; normales Sitzungslog unter `sd:/pcsx2/logs/` privat auswerten
oder zurückgeben. Benchmark 1.1 schreibt eine neue `wii-limits-XXXX.csv` unter
`sd:/gekko2-bench/`. Diese beiden Messungen dienen unterschiedlichen Zwecken.
