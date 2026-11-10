# BETA12.26 – gemeinsame GS→GX-Funktionen

Basis: `b2c0a20fe01e85e991177130a110da9b6f69b22e` (BETA12.25).
Branch: `codex/beta12-26-generic-gx`. Experimenteller Wii-Teststand.

## Unterstützung nach GS-Funktion, nicht nach Spiel

Alle Spiele schreiben dieselben GS-Register und schicken dieselben
Primitivtypen. Der Renderer entscheidet anhand dieses Zustands, ob ein
GX-Pfad seine Semantik unterstützt. Es gibt hier keine Dateinamen-, Titel-,
CRC- oder Seriennummern-Auswahl und keinen separaten Tekken-/Crash-Renderer.
Ein korrigierter gemeinsamer Pfad steht jedem Spiel mit diesen Zuständen zur
Verfügung. Spielaufnahmen helfen beim Finden fehlender Zustände; sie sind
kein Anlass für einen eigenen Port jedes Spiels.

Ein allgemeiner Renderer benötigt dennoch mehrere Implementierungen:
GX hat andere Texturformate, Farb-/Blendpräzision und EFB-Zugriffsregeln als
der PS2-GS. Nicht unterstützte Zustände bleiben auf dem Software-GS bzw.
vorhandenen Hybridpfaden. Bloßes Erzwingen von GX kann Masken, Alpha, Z und
Framebuffer-Abhängigkeiten verlieren. Auch EE-/VU-/GIF-/IRQ-Probleme werden
durch eine Renderer-Auswahl nicht automatisch behoben.

## Neue gemeinsame Pfade

| GS-Funktion | BETA12.26 | Grenzen |
|---|---|---|
| MODULATE, DECAL, HIGHLIGHT, HIGHLIGHT2 | gemeinsame GX-TEV-Stufen nach Texturfilterung | experimentelle GX-RGB-Präzision |
| TCC und wechselndes Vertex-Alpha | exakte CPU-Alpha-Berechnung und kanonischer Alpha-Schatten | AS-abhängige Mischungen mit veränderlicher resultierender Alpha bleiben eingeschränkt |
| Fog | eigenes Vertex-Attribut, nach TFX und vor Source-Alpha-Addition | GX-RGB-Präzision; keine native Kombination mit vorab skaliertem FIX-Additiv |
| Untexturierte Dreiecke | GX-Farb-Passthrough, variables Z, GS-Alpha-/DATE-/AFAIL-Tests | CT32/CT24-Ziel und unterstützte Mischungen |
| REPEAT, CLAMP, REGION_CLAMP, REGION_REPEAT | gemeinsame dekodierte Atlasquelle | konservative VRAM-/Z-Aliasprüfung und Größenlimit |
| 1- und 2-Texel-Achsen | auf mindestens vier Texel gepolstert | gleiche Alias-/Größenprüfung |
| Alle 13 GS-Quellformate | vorhandener kanonischer Decoder auch für Atlasquellen | Upload als GX-RGBA8, nicht 13 native GX-Formate |

Für HIGHLIGHT/HIGHLIGHT2 wird das interpolierte Vertex-Alpha nach MODULATE
zur RGB-Farbe addiert. Die TCC-abhängige Zielalpha wird getrennt berechnet.
Fog erhält einen eigenen Farbkanal statt Vertex-Alpha als Fog-Ersatz zu
missbrauchen. Zwei Fog-Stufen mischen FOGCOL und die fertige Quellfarbe und
berücksichtigen, dass die Software-GS-Foggewichte zusammen 255/256 ergeben.

Die Atlasplanung trennt wiederholte Atlasgröße und tatsächlich gelesene
GS-Koordinaten. Bei REGION_REPEAT kann z. B. ein kurzer Wiederholbereich auf
weit entfernte GS-Texel zeigen. Deshalb umfasst die physische Aliasprüfung
`UMSK | UFIX` bzw. `VMSK | VFIX`, nicht nur TEX0.TW/TH oder die Atlasgröße.
Read-only-Z-Aliase werden ebenfalls konservativ abgelehnt. Geänderte Clamp-,
Paletten- und VRAM-Daten verändern den Quellschlüssel. Die vorhandene
CLAMP-Konfigurationsschutzbedingung bleibt bestehen.

Im gemeinsamen CT32-Dreieckspfad wird die Quellalpha für Test und Schattenzustand
nur einmal je Fragment berechnet. Konstante Vertexalpha benötigt keine
erneute baryzentrische Alpha-Interpolation. Diese gemeinsame Optimierung gilt
auch im Kontrollbuild; dessen Zulassungsumfang bleibt auf BETA12.25.

Die Texturmatrix wird auf die Atlasgröße umgerechnet; FST-UV und STQ behalten
dabei ihre ursprünglichen GS-Koordinaten. Negative Koordinaten, Clamp-Grenzen
und bilineare Taps werden konsistent übersetzt. Ungültige Regionintervalle,
mehr als 1024 Texel je Achse, mehr als 524.288 Upload-Texel, 4-MiB-Wrap oder
unsichere Aliase kehren zum bestehenden GS-Pfad zurück.

## Was offen bleibt

Keine vollständige GX-Abdeckung sämtlicher GS-Zustände. Mipmaps/LOD,
CT16-/CT16S-Ziele dieses nativen Dreieckspfads, FBMSK, Dithering,
COLCLAMP-Wrapping, komplexe Blendgleichungen/Framebuffer-Feedback sowie
weitere native Punkt-/Linien-/Spritekombinationen bleiben offen bzw. hybrid.
Native GPU-Alpha-/Z-Prüfung ist nicht universell implementiert: die CPU
berechnet diese Berechtigungen weiterhin und hält den exakten GS-Zustand.

Der neue Pfad verändert keine Auflösungskorrektur, SIGNAL-Quittierung,
Schedulerzyklen oder Spielassets. Fragezeichen-Kacheln, Crash-Fortschritt,
Tekken-Kampf und echte Wii-FPS sind damit nicht als behoben/beschleunigt
bestätigt. Texturfilterung/Gouraud/Blend-RGB sind weiterhin experimentell.

## Tests und ihre Aussagekraft

- `tools/test_beta12_26_host.py --output-json <Datei>`: 54 Host-Suites.
- 524.288 TFX/TCC-Alpha-Paare gegen die vorhandene Software-GS-Texturfunktion.
- 479.232 Atlas-/Software-Samples über alle 13 Formate, vier Clamp-Modi,
  acht Achsgrößenkombinationen, nearest/bilinear und negative/Grenzkoordinaten.
- GIF-Zulassung und Quellschlüssel: Fog-Attribute, HIGHLIGHT, wechselndes
  Alpha, untexturierte Dreiecke, kleine Texturen, Regionkoordinaten,
  VRAM-/Clamp-/Palettenänderung und deaktiviertes Runtime-Gate.
- Bestehende 81.920 GS-Fragment-/Raster-Differentialfälle und 24.576
  Berechtigungsfälle bleiben Teil der Regressionen.
- `verify_gx_common_beta12_26.py` führt den gelinkten PPC-Code aus: 728
  exakt geprüfte Alpha-Schattenpixel, tatsächliche TEV-Stufenkonfiguration,
  Fog-Farbregister/Attribute im FIFO, untexturierter Farb-Passthrough,
  FIX-Zulassung und sicherer AS-Rückfall. Der enthaltene BETA12.25-Test
  prüft zusätzlich variable Z-Werte, DATE, AFAIL, Cache und 128×1024.
- Ein CPU-Modell der erfassten TEV-Konfiguration zeigt in diesen konstanten
  RGB-/Nearest-Testfällen maximal 2/255 Abweichung vom Software-GS.
  Das ist weder eine allgemeine Fehlergrenze noch echtes GX-Readback;
  bilineare Hardwarefilterung, andere Farben und Interpolation müssen
  gesondert auf GPU/Hardware geprüft werden.
- DOL-/ELF-Prüfung und statische MEM1-Prüfung für gemeinsamen und
  BETA12.25-Kontrollpfad. Gelinkte PPC-Prüfungen der bisherigen Formate,
  Shading-/Ownership-Pfade, Fences und Cold Boot ergänzen die Regressionen.

Es wurden keine PS2-BIOS-/Disc-Dateien, Benutzerlogs oder proprietären
Traces in die Tests oder Veröffentlichung aufgenommen.

## Build und Vergleich

SDK: `devkitpro/devkitppc@sha256:4c919aa26151dd43d88ca28c922d1fe2409579a8ba60ef56517baf1abdfb1a48`,
devkitPPC r50 / GCC 16.1.0 / libogc 3.1.0.

```
bash tools/build_beta12_26_gx.sh
GX_GENERIC=0 bash tools/build_beta12_26_gx.sh
```

Der erste Build enthält `GEKKO2_GX_GENERIC_SHADER` und
`GEKKO2_GX_EXTENDED_TRIANGLES`. Der zweite lässt ausschließlich die neuen
Gemeinschaftsfunktionen weg und nutzt den BETA12.25-Zulassungspfad. Beide
verwenden getrennte Targets/Buildverzeichnisse und dieselben SPEED/DSI/AOT-,
Trace- und standardmäßig Chain-kompilierten Optionen. Die gespeicherten
Chain-/PSM-Experimente bleiben standardmäßig OFF; vorhandene SD-Optionen
werden nicht stillschweigend überschrieben. Volle Bildgröße bleibt erhalten.

Runtime-Gates bleiben GX_GOURAUD und GX_RESIDENT. Im Wii-Log erscheinen
`GX_COMMON_SHADER compiled=... untextured=... highlight=... fog=... varying_vertex_alpha=...`
und die bestehenden `GX_EXTENDED_TRIANGLES`-/`GS_GOURAUD`-Zähler.

Kontrolle und neuen Build mit gleichem Image, identischen Optionen und je
Cold Boot vergleichen. Der Anstieg eines akzeptierten GX-Zählers beweist
nur eine andere Route, keine höhere Gesamt-FPS oder Spielbarkeit.
