# BETA12.30 – geprüfte Downloads

Entwicklungs-Teststand, kein abgeschlossener Emulator.

- [Gekko2-BETA12.30-SD.zip](https://raw.githubusercontent.com/Mafiacoding/Gekko2/1e9f5925ecd649b225d0d34becab4490a3785bb4/downloads/Gekko2-BETA12.30-SD.zip) – 914755 Bytes.
  SHA-256 `f99c67075660b13dda1cf843f86b229bfab83d00cde261f0f013a650d4f5cd96`.
- [Gekko2-BETA12.30-review.zip](https://raw.githubusercontent.com/Mafiacoding/Gekko2/1e9f5925ecd649b225d0d34becab4490a3785bb4/downloads/Gekko2-BETA12.30-review.zip) – 17446927 Bytes.
  SHA-256 `54b5f63b37e34c66cb26a14941c39d9a112910fd768a782163aa189c9f283456`.

Unveränderlicher Artefaktcommit `1e9f5925ecd649b225d0d34becab4490a3785bb4`, Quellstand
`82358c3c6a4f22f163b8cab002dd0e72829af3de`, Emulator-Buildcommit `640a761`.
Beide URLs nach Veröffentlichung erneut abgerufen: HTTP 200, bytegleich mit
lokalem Paket, SHA-256 und vollständige ZIP-CRC bestanden.

SD-ZIP enthält `apps/Gekko2/boot.dol`, Metadaten und Icon. Review-ZIP enthält
beide DOL-/ELF-Varianten, Quellen, Buildprovenienz und synthetische Prüfberichte.
Keine BIOS-/Spieldateien, Benutzerkarten, privaten AOT-Profile oder Rohlogs.
Chain, PSM und GX-Risk ab Werk OFF; bereits gespeicherte Einstellungen bleiben
erhalten. Keine automatische Neuformatierung einer vorhandenen Karte.

78 Host-Suites, 14 gelinkte PPC-Prüfgruppen und zwei Wii-Cross-Builds samt
DOL-/ELF-/statischem MEM1-Gate bestanden. Kein physischer Wii-/Dolphin-Lauf,
kein bestätigter Crash-/Tekken-/BIOS-Kachel-/FPS-Fix.
[Änderungen, Grenzen und Reproduktion](BETA12.30-GS-VIF-ACCURACY.md).
