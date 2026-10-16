# BETA12.27 – GX-Risikomodus

Quellcommit: `c51ab567f651195beb0470b11419d8b7eafa4fd8`, Branch `codex/beta12-27-gx-risk-mode`.
Wii-Testbuild: `Gekko2-BETA12.27-GX-Chain.dol`.

Im Launcher: Settings → CPU/GX Optimizations. Vom ersten Eintrag UP drücken,
GX RISK MODE - FAST TRIANGLES auswählen, A/Links/Rechts zum Umschalten.
Standard OFF; gespeichert in sd:/pcsx2/gx-risk.cfg und erst beim nächsten
Cold Boot aktiv. START/Resume übernimmt die Änderung nicht. Chain/PSM bleiben
unabhängig; die SD-ZIP überschreibt keine Konfigurationsdateien.

ON darf geeignete Dreiecke einmal komplett an GX geben, wenn alle bedeckten
Pixel RGB schreiben dürfen und denselben PABE-Blend-Gate haben. Die direkte
GX-Kantenabdeckung kann Grafikfehler erzeugen. Unterschiedliche Pixel-Freigaben,
Scanmasken und nicht unterstützte Zustände verwenden weiterhin den bisherigen
Pfad. Speicher-/Aliasprüfung, CPU-Alpha/Z und Fences bleiben aktiv.

55/55 Host-Suites, 131.072 neue Farb-/PABE-Masken und vier zusätzliche
Options-Buildprofile bestanden. Gelinkter PPC-Test: im synthetischen Fall
OFF=16 GX-Übermittlungen, ON=1, gleiche Alpha-/Z-Metadaten. Cold-Boot-Isolation,
ungepolsterte BBox und PABE/ATE/DATE/Z/SCANMSK-Rückfälle bestanden. Enthaltene
BETA12.25-/12.26-Regressionsfälle sowie Formate/Ownership/Fences/Cold Boot
bestanden. GX-Aufrufe simuliert; echte Kantenpixel und Wii-FPS nicht geprüft.

SDK: devkitpro/devkitppc@sha256:4c919aa26151dd43d88ca28c922d1fe2409579a8ba60ef56517baf1abdfb1a48.
devkitPPC r50 / GCC 16.1.0 / libogc 3.1.0. DOL-/ELF- und statische MEM1-Prüfung
bestanden. Genaue Endadresse/Reserve in wii.txt. Kein Runtime-Heap-Nachweis.
Bestehende SPEED/DSI/AOT-Flags, volle Ausgabegröße und GX-Fehlerbehandlung bleiben.

SD-ZIP auf SD entpacken: eigene App apps/gekko2-beta12-27; vorherige App bleibt
erhalten. OFF und ON mit gleichem Spiel, gleicher Szene und identischen übrigen
Optionen vergleichen, jeweils Cold Boot. Bei Grafikfehlern OFF speichern und
Cold Boot. Logzeile GX_RISK_MODE zeigt requested/enabled und Übermittlungszähler.

Die neue Zulassungsprüfung benötigt CPU-Zeit. Weniger GX-Befehle sind kein
FPS-Versprechen. Kein bestätigter Wii-Gameplay-/Kachel-/Crash-Fix. Keine BIOS-/
Disc-Dateien, Benutzerlogs oder proprietären Traces im Paket. Details in
DIAGNOSTICS.md; Quellcode, Patch, ELF und synthetische Prüfausgaben im Review-ZIP.
