# BETA12.25 – Wii-Testbuilds

Quellcommit: `b2c0a20fe01e85e991177130a110da9b6f69b22e`, Branch `codex/beta12-25-tekken-gx`.

Experimentell: `Gekko2-BETA12.25-GX-Chain.dol`.
Kontrolle ohne GEKKO2_GX_EXTENDED_TRIANGLES: `Gekko2-BETA12.25-GX-Chain-Control.dol`.
Beide enthalten dieselben SPEED/DSI/AOT/Game-Trace-Flags und Chain-Unterstützung.
Chain/PSM sind ab Werk OFF; vorhandene SD-Einstellungen werden nicht verändert.
Für einen isolierten Vergleich beide Optionen in beiden Runs OFF, dann Cold Boot.

52/52 Host-Suites bestanden. 81.920 neue Vergleiche mit dem Software-GS sowie
24.576 Berechtigungs-/Texturgrenzfälle. Gelinkte PPC-Prüfungen für neue
Dreiecksbefehle, Shading/Ownership, alle 13 Texturformate, DISPLAY/Readback,
Fences und Cold Boot bestanden. GX-Hardwareaufrufe sind in diesen Prüfungen
simuliert; GPU-RGB-Pixel werden nicht auf echter Hardware verglichen.

Beide DOLs mit devkitPPC r50/GCC 16.1.0/libogc 3.1.0 gebaut.
SDK: devkitpro/devkitppc@sha256:4c919aa26151dd43d88ca28c922d1fe2409579a8ba60ef56517baf1abdfb1a48.
DOL-/ELF-Sektionsgleichheit, Wii-Entry und statische MEM1-Prüfung bestanden.
Die genauen Endadressen/Reserven stehen am Ende von wii.txt / wii-control.txt.
Statische Reserve ist kein Beleg für ausreichenden Heap bei jedem Spiel.

SD-ZIP direkt auf SD entpacken: zwei getrennte Apps im Homebrew Channel.
Vorhandene Installation bleibt als Rückfallversion bestehen. Keine SD-Konfig,
BIOS- oder Spieldateien im Paket. Dieselbe Szene und dieselben Optionen zuerst
mit Kontrolle und dann mit GX testen; Logs/Screenshot vergleichen.

Noch nicht vollständig GX: Fog, Mipmaps, Region-Clamps, HIGHLIGHT, variable
Vertexalpha, komplexe Masken/Blends/Framebuffer-Feedback und weitere Pfade.
Siehe DIAGNOSTICS.md. Keine bestätigte Korrektur der Fragezeichen-Kacheln,
kein bestätigter Tekken-Kampf, kein gemessener Wii-FPS-Gewinn.
