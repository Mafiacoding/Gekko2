# BETA12.25 – experimenteller GX-Dreieckspfad für Tekken

Basis: `eaccd9853a6559432705b514ee0a6544bf721b65` (BETA12.24a), Arbeitsbranch
`codex/beta12-25-tekken-gx`. Kein Merge nach main und kein Hardware-Spielbarkeitsbeleg.

## Implementiert

Die bisherigen nativen Texturdreiecke lehnten große Texturen, Alpha-/DATE-Tests
und Z-Schreibzugriffe bei veränderlichem Z ab. Der neue, getrennt kompilierbare
Pfad erweitert diese Kombinationen für CT32-/CT24-Ziele:

- GX übernimmt Texturfilterung, projektive STQ-Abbildung, RGB-Gouraud/MODULATE
  oder DECAL und unterstützte native Blendgleichungen.
- Zweierpotenz-Texturen von mindestens 4×4 bis 1024 je Achse, insgesamt maximal
  524.288 Texel (2 MiB GX-RGBA8). Damit ist insbesondere 128×1024 zugelassen.
  Die vorhandenen Decoder aller 13 GS-Texturformate bleiben erhalten.
- Die CPU berechnet GS-Fragmentberechtigungen: alle acht Alpha-Tests, vier
  AFAIL-Modi, DATE/DATM, vier Z-Tests, FBA und die kanonischen Z-Schreibwerte.
  Getrennte durchgelassene Bereiche einer Zeile werden einzeln gezeichnet;
  abgelehnte Pixel werden nicht durch ein verbreitertes Rechteck übermalt.
- CT32-Alpha bleibt ein exakter GS-Schattenzustand; CT24 erhält den vorhandenen
  Alpha-Speicher. Z wird mit den ursprünglichen drei Vertexwerten interpoliert,
  auf das GS-Z-Format begrenzt und unabhängig von der RGB-Ausgabe geschrieben.
- PABE kann bei der unterstützten Source-Alpha-Mischung innerhalb eines
  Dreiecks zwischen Kopieren und Blenden wechseln. Andere PABE-Blendformen
  bleiben im Softwarepfad.
- Ein nativer Texturcache berücksichtigt VRAM-Inhalt, Format, TEXA, logische
  Palette und Texturtransformation. Er überlebt nur nachweislich disjunkte
  GPU-Schreibbereiche. Uploadbuffer-Nutzung für andere Quellen, Präsentation,
  Cold Boot, geänderte Schlüssel oder Transformationen verhindern veraltete
  Treffer. Eine unbekannte GPU-Ownership invalidiert weiterhin konservativ.
- Kleine Dreiecke verwenden dieselbe Aliasprüfung ohne das alte
  512-Boundingbox-Pixel-Minimum. Unveränderte Texturen können wiederverwendet
  werden, statt sie für jedes kleine Dreieck komplett zu dekodieren.

Keine Prüfung hängt vom Namen der Spieldatei ab. Der vorherige native
Zulassungspfad bleibt erhalten. Unsupported-Zustände gelangen weiterhin über
Ownership-/Readback-Barrieren zum Software-GS. Die volle Bildausgabe aus
BETA12.24a bleibt erhalten; es gibt keine neue automatische Verkleinerung.

## Bewusst noch offen

**Tekken ist damit nicht vollständig auf GX umgesetzt.** Fog, Mipmapping,
REGION_CLAMP/REGION_REPEAT, HIGHLIGHT/HIGHLIGHT2, veränderliche Vertexalpha,
CT16/CT16S als Ziel dieses nativen Dreieckspfads, FBMSK, COLCLAMP-Wrapping,
komplexe Blendgleichungen und Framebuffer-/Textur-Feedback benötigen weitere
native Implementierungen bzw. verbleiben in vorhandenen Hybrid-/Softwarepfaden.
Auch Punkt-/Linien-/Spritepfade sind nicht durchgehend reine GPU-Ausführung.
Die Texturdekodierung und GS-Pixelberechtigungen laufen weiterhin auf der CPU.

GX-Filter-, Farbinterpolations- und Blendpräzision sind experimentell und nicht
bitgleich mit dem PS2-GS bewiesen. Die neuen Pixelberechtigungen ersetzen keine
GPU-RGB-Differentialmessung. Zu viele unterteilte Zeilen können zusätzlichen
GX-Befehlsaufwand erzeugen; eine Wii-Messung muss den Nettogewinn zeigen.

Die Fragezeichen-Kacheln in der Charakterauswahl, Tekkens Übergang zum Kampf,
Crashs SIGNAL-/XGKICK-Stall und BIOS-Grafikfehler sind durch diese Änderung
**nicht auf echter Wii als behoben bestätigt**.

## Reproduzierbarer Build

SDK: `devkitpro/devkitppc@sha256:4c919aa26151dd43d88ca28c922d1fe2409579a8ba60ef56517baf1abdfb1a48`,
devkitPPC r50, GCC 16.1.0, libogc 3.1.0.

```
bash tools/build_beta12_25_gx.sh
GX_EXTENDED=0 bash tools/build_beta12_25_gx.sh
```

Die zweite Variante hat einen eigenen Target-/Buildverzeichnisnamen und lässt
`GEKKO2_GX_EXTENDED_TRIANGLES` weg. SPEED/DSI/AOT, GAME_TRACE und die bestehende
experimentelle native GX-Unterstützung bleiben gleich. Beide Builds enthalten
standardmäßig Chain-Residenz-Unterstützung; deren gespeicherte Runtime-Option
sowie Experimental PSM Conversion behalten ihren Standard OFF. Mit
`CHAIN_RESIDENCY=0` ist weiterhin eine Variante ohne Chain-Kompilierung möglich.

Laufzeitgates bleiben `GX_GOURAUD` und `GX_RESIDENT`. Die Buildkontrolle ist der
reproduzierbare Vergleich des neuen Pfads, kein neuer Cold-Boot-Menüschalter.

## Prüfungen

- `python3 tools/test_beta12_25_host.py --output-json <Datei>`: 52 Host-Suites.
  Darin 24.576 unabhängige Berechtigungskombinationen und 81.920 direkte Vergleiche
  gegen den vorhandenen Software-GS-Rasterizer, einschließlich vier Z-Formaten.
- GIF-Zulassungstest prüft kleine 128×1024-Dreiecke mit variablem Z, Alpha/DATE,
  MODULATE/DECAL, TCC sowie den Rückfall bei Fog und Textur-/Framebuffer-Alias.
- Der alte Shading-Test verwendet jetzt korrekt `double` für die seit BETA12.23
  geänderte UV-Struktur; 19.584 Quellpixel werden gegen den Software-GS geprüft.
- `verify_gx_extended_beta12_25.py`: führt den real gelinkten PPC-Code aus;
  überprüft fragmentierte Alpha-Bereiche, interpoliertes Z, DATE, ZB_ONLY,
  PABE-Befehle, Texturcache und 1024-hohe Texturen. GX-Aufrufe sind simuliert;
  es werden keine echten GPU-RGB-Pixel oder FPS verifiziert.
- Bestehende gelinkte PPC-Prüfungen für Quellshading, 13 Texturformate,
  Präsentations-/Importformate, Fences und Cold Boot ergänzen die Regressionen.
- DOL-/ELF-Sektionsgleichheit, Wii-Einstiegspunkt und statisches MEM1-Limit
  werden für beide DOLs geprüft. Das ist kein Runtime-Speicherstresstest.

Konkrete Ausgaben, Hashes und Speicherwerte gehören zum veröffentlichten
Buildpaket. Keine BIOS-/Disc-Dateien, Benutzerlogs oder proprietären Traces
werden mitgeliefert.

## Wii-Vergleich

Zuerst Kontrollbuild, dann erweiterten Build mit demselben Spiel, denselben
SD-Optionen und je einem Cold Boot ausführen. Möglichst dieselbe Szene und
vergleichbar lange Abschnitte verwenden. GAME_TRACE enthält die bestehende
Ereignishistorie. Neue Zeilen:

```
GX_EXTENDED_MODE compiled=... gate=... precision=...
GX_EXTENDED_TRIANGLES accepted=... bbox_pixels=... fragment_checks=... color_rejected=... z_permissions=... source_cache_hits=... attempts=... GPU_vertices=...
```

Mit `SESSION`-Zeitpunkten, `GS_GOURAUD` und den Retire-/Output-Zählern vergleichen.
Ein Anstieg nativer Draws ist allein kein FPS- oder Spielbarkeitsnachweis.
Nach einem GX-Fehler bleibt der sichere Fehlerpfad erhalten. Keine Änderungen
an SIGNAL-Quittierungen oder erzwungenen Spiel-Fortschritten.
