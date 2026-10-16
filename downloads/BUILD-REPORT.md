# BETA12.26 – gemeinsamer GS→GX-Teststand

Quellcommit: `dea99ddabc79836440f4b812f06bca3fe47617d0`, Branch `codex/beta12-26-generic-gx`.
Experimentell: `Gekko2-BETA12.26-GX-Chain.dol`.
Kontrolle mit BETA12.25-Zulassung: `Gekko2-BETA12.26-GX-Chain-Beta25Control.dol`.
Beide behalten SPEED/DSI/AOT/Game-Trace, volle Bildgröße, GX-Fehlerbehandlung
und Chain-Unterstützung. Chain/PSM ab Werk OFF; bestehende SD-Werte bleiben
unverändert. Für einen isolierten Vergleich beide Optionen in beiden Runs
OFF einstellen und Cold Boot ausführen.

Gemeinsame Erweiterungen: Fog, HIGHLIGHT/HIGHLIGHT2, geeignete variable
Vertex-Alpha-Kombinationen, untexturierte Dreiecke, alle vier Clamp-Modi,
gepolsterte 1-/2-Texel-Achsen. Kein eigener Renderer nach Spielnamen.
Beide Builds berechnen die CT32-Quellalpha nur einmal je Fragment.

54/54 Host-Suites bestanden. Neue Quellprüfungen: 524.288 TFX/TCC-Alpha-Paare
und 479.232 Atlas-/Software-Samples über alle 13 GS-Quellformate. Dazu die
bisherigen 81.920 direkten GS-Fragment-/Raster-Vergleiche und 24.576
Berechtigungsfälle. Gelinkte PPC-Prüfungen: exakter Alpha-/Z-Schatten,
TEV-Konfiguration, FIFO-Attribute, Fog/HIGHLIGHT, Cache, 128×1024, alle
Quellformate, Ownership/Fences und Cold Boot. Auch der Kontroll-DOL wurde
mit dem gelinkten BETA12.25-PPC-Test geprüft.

GX-Aufrufe sind in diesen PPC-Prüfungen simuliert. Das CPU-Modell der
aufgezeichneten TEV-Stufen zeigt in seinen konstanten RGB-/Nearest-Fällen
maximal 2/255 Abweichung. Das ist keine allgemeine oder hardwaregemessene
RGB-Grenze und kein FPS-Messwert.

SDK: devkitpro/devkitppc@sha256:4c919aa26151dd43d88ca28c922d1fe2409579a8ba60ef56517baf1abdfb1a48.
devkitPPC r50, GCC 16.1.0, libogc 3.1.0. Beide DOLs bestehen die
DOL-/ELF-Sektions-, Wii-Einstiegspunkt- und statische MEM1-Prüfung.
Endadressen/Reserven stehen in wii.txt und wii-control.txt. Keine Runtime-
Heap- oder echte Wii-Spielbarkeitsbestätigung.

SD-ZIP auf SD entpacken: zwei getrennte Apps im Homebrew Channel. Vorhandene
Installation bleibt erhalten. Dasselbe Spiel und dieselbe Szene mit gleichen
Optionen zuerst mit Kontrolle und dann mit COMMON GX testen. Die Zähler
GX_COMMON_SHADER und GX_EXTENDED_TRIANGLES zeigen die genutzten Pfade.

Offen: Mipmaps/LOD, native CT16-/CT16S-Dreiecksziele, Masken/Dithering,
komplexe Blends/Framebuffer-Feedback und weitere Punkt-/Linien-/Spritefälle.
Siehe DIAGNOSTICS.md. Keine bestätigte Kachelkorrektur, Tekken-Kampf,
Crash-Fortschritt oder höhere Wii-FPS. Keine BIOS-/Disc-Dateien, Benutzerlogs,
proprietären Traces oder überschreibenden SD-Konfigurationsdateien im Paket.
