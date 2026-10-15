# R1388 – nativer GX-Testpfad und GS-CPU-Cache

Der GS rasterisiert Farben und Elemente. EE/VU erzeugen Geometrie und
Zeichenbefehle; GIF überträgt sie. IOP übernimmt Geräte-/Systemaufgaben.
Ein schnellerer EE-JIT ersetzt deshalb keine GPU-Rasterisierung im GS.

R1387-Log, TIME_SAMPLE TB_total=8951034, GS_raster=8102632:
90,5218 % der erfassten CPU-Zeit dieses Intervalls lagen im Software-GS.
Das ist kein Anteil des gesamten Laufs. Andere Intervalle werden hauptsächlich
von EE, IOP und Scheduler bestimmt. RAM ist Cache, kein Rechenprozessor.

## Implementierung

Der GPU-Testbuild übergibt echte Dreiecke an GX statt Bildschirmtexel vorher
auf der CPU zu schattieren. GX übernimmt Texturabtastung mit Nearest/Linear,
REPEAT/CLAMP, perspektivische STQ-Division und ausgewählte Gouraud-Modulation.
S, T und Q werden als affine Ebenen aus den drei Vertices berechnet. Eine
GX-3x4-Texturmatrix aus der Bildschirmposition erledigt die Projektionsdivision;
der Vertex-Z-Wert wird dafür nicht zweckentfremdet.

Unterstützte Farbpfade: konstant GS-vorschattierte Quelltexel oder variable
Vertex-RGB bei MODULATE/TCC mit gleicher Vertex-Alpha. Native additive
FIX-Blends und klassisches Source-over (A=source, B=destination, D=destination,
C=source-alpha oder FIX) laufen in GX. Source-over erlaubt Koeffizienten bis
128; sie werden auf die GX-255-Skala abgebildet. Die ursprüngliche GS-Alpha
bleibt als separate Metadaten erhalten. Fertige Farben bleiben im residenten
EFB bis ein notwendiger Speicherzugriff sie synchronisiert.

Der Testbuild aktiviert GX_GOURAUD und GX_RESIDENT für die Sitzung auch bei
alten deaktivierten Einstellungen; die gespeicherte Konfiguration bleibt
unverändert. Der Kontrollbuild respektiert sie weiter. EE/IOP/VU-JIT und AOT
bleiben erhalten und laufen auf der CPU.

Auch der Kontrollbuild spart Arbeit: ein zeichnungsbezogener Texturcache
wird nun bei Dreiecken verwendet, wenn Textur, Ziel und Z-Puffer nachweislich
nicht überlappen. Gleiche drei Vertexfarben benötigen keine redundant
interpolierten Farbkanäle. Nach jedem Draw endet die Cache-Gültigkeit.

## Bewusste Grenzen

GX-GPU-Filter-/TEV-/Blend-Rundung ist nicht bitgleich mit dem PS2-GS. Deshalb
ist der native Pfad ein ausdrücklich experimenteller Testbuild. Kein
Brightness-Hack, kein hinzugefügter BIOS-Schriftzug und keine erfundenen Kacheln.

Z-Prüfung/CPU-Abdeckung und GS-Alpha bleiben zunächst hybrid. Konstant-Z
verwendet die bestehende GS-Depth-Maske. Variierendes Z wird nur ohne Z-Schreibzugriffe
und bei Z32 akzeptiert, wenn eine konservative untere Grenze jedes abgedeckte
Sample bestehen lässt; jeder Fehltest führt zurück zum Scalarpfad.
Z wird nicht blind ausgeschaltet. Variable Alpha, andere Blendgleichungen,
Alpha-/Destination-Tests, Masken, Fog, Mipmaps, Alias-/Layoutfälle und
Texturen außerhalb 4..128 je Achse fallen weiter zurück. Quelltexel werden
pro Draw dekodiert; Upload-/Framebuffer-Synchronisation benötigt weiterhin CPU.
Dreiecke werden nach gleichen GS-Zeilenmasken gruppiert und gescissort.
Der Build verschiebt daher noch nicht alle Grafikarbeit auf GX.

## Prüfung und Messgrenzen

- Zehn Host-Suites: Texturfunktion, STQ/Sprite, Linearfilter, GIF-Q,
  Q-Checkpoint, PACKED TEX0/CLAMP, Kontext 2, CLAMP, DATE und Hardware R1384.
- Tatsächlich verlinkter PPC-Code: native Dreieck-Submission, Projektionsmatrix
  an drei Vertices, Gouraud-Übergabe, GS-Alpha-Metadaten, ungültiges Q/Alpha,
  konservative Z-Freigabe und Rückfall bei Tiefenfehler sowie GX-Source-over.
- Beispiel: 16x16 Quelltextur = 256 CPU-Sampling-Aufrufe; Dreiecks-Bounding-Box
  64x64 = 4096 Pixel. Das ist ein Nachweis der entfernten Bildschirm-Sampling-
  Schleife, keine 93,75-%-Gesamtbeschleunigung. Coverage/Z und GX-Aufrufe bleiben.
- Sieben verlinkte GIF-Q-Fälle, 53 PPC-GS-Speicherfälle und der bestehende native
  Additiv-Submissionstest bestanden. GX wird in diesen Tests modelliert;
  physische EFB-Pixel und Wii-FPS sind nicht gemessen.
- Beide DOLs werden mit ihren ELF-Sektionen verglichen und auf MEM1-Grenzen geprüft.
  Moderne devkitPPC-r50/GCC-16.1.0/libogc-3.1.0-Toolchain wie zuvor.

R1386 enthält keinen verwertbaren Exception-Dump für den gemeldeten Crash.
R1387 endet mit EXIT_REQUEST/EXIT_HBC; daraus folgt keine generelle Crashfreiheit.
BIOS-Kacheln/Dunkelheit, Tekken-Black-Screen, GT3-Rennen und Ziel-FPS sind offen.
Frühere IRQ-, VU1-Reset-, Q- und AOT-Fixes bleiben integriert.

## Wii-Test

ZIP auf die SD-Karte entpacken, vorhandenes eigenes BIOS und Memory-Card-Daten
weiterverwenden. Im Homebrew Channel zuerst **Gekko2 R1388 GPU Test** wählen;
**Gekko2 R1388 Control** ist der Vergleichsbuild. Beide haben eigene App-Ordner.
Keine optimization.cfg, BIOS-/Game-/Memory-Card-Datei wird mitgeliefert.

Im neuen Log müssen GX_NATIVE_MODE enabled=1 sowie GX_NATIVE_TEXTURE mit
accepted, STQ, Gouraud, CPU_texels und screen_bbox_pixels erscheinen. accepted=0
bedeutet: diese Szene nutzt den neuen Pfad noch nicht. Nicht Präsentations-Hz
mit Gast-FPS verwechseln. TIME_SAMPLE GS_raster, GX_upload/GX_wait und die
Übernahmezähler gemeinsam mit den Bildern auswerten.

Quellen: gebündelte PCSX2 GSState/GSDrawScanline; moderne libogc gx.h,
GX_TG_MTX3x4 und GX_LoadTexMtxImm; vom Nutzer gelieferte R1386/R1387-Logs.
