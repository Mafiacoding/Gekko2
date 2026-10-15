# R1389: echte BIOS-GX-Übernahme statt Alpha-Rückfall

## Befund aus dem Nutzerlog

R1388: GX_NATIVE_MODE enabled=1, 29.206 Versuche, accepted=0,
93.839.360 dekodierte CPU-Quelltexel, 22.910 Alpha-Ablehnungen.
Der native Dreieckpfad war eingeschaltet, übernahm aber nichts. Der ältere
additive GX-Pfad war aktiv; dessen Bildschirmtexel wurden weiter auf der CPU
berechnet. Die beiden Pfade dürfen nicht miteinander verwechselt werden.
R1388 endete mit EXIT_HBC; das bestätigt keinen behobenen BIOS-Crash.
Das PCSX2-Referenzbild ist lesbar. Fehlende Kacheln sind weiterhin ungeklärt.

## Implementierte Änderungen

1. Variable Textur-Alpha verhindert nicht länger jede native CT32-Übernahme.
   Raw-Quelltexel werden einmal pro Draw dekodiert. GX übernimmt RGB-Sampling,
   STQ-Division, unterstützte Farbmodulation und Blending. Die ursprünglichen
   Alpha-Texel bleiben in einem 16-KiB-Puffer; daraus berechnet der CPU-Schatten
   ausschließlich GS-Alpha mit Nearest beziehungsweise dem GS-4-Bit-Bilinearfilter.
   TCC/TFX werden auf die gefilterte Alpha angewandt. RGB-MODULATE wird einschließlich konstanter Vertexfarbe erst nach dem
   Filtern in GX ausgeführt. RGB und Blending werden dabei nicht pro
   Bildschirm-Pixel auf der CPU gerechnet.
2. Einfarbige endgültige Alpha überspringt den Alpha-Pixel-Loop. Unterschiedliche
   Quell-Alpha bedeutet bei TCC=0 oder gesättigter TFX nicht automatisch
   unterschiedliche Ausgabe-Alpha. Metadata bleiben auch bei CT24 korrekt.
3. Der residente Farbtarget verwendet die konfigurierte GS-SCISSOR-Höhe,
   auf vier Zeilen aufgerundet und auf die EFB-/GS-Grenzen beschränkt.
   Echte BIOS-Pakete setzen 256 Zeilen. Der bisherige 512-Zeilen-Bereich
   erkannte eine falsche Überlappung mit dem benachbarten Z-Puffer.
   Die Aliasprüfung und GS-Z-Prüfung bleiben aktiv.
4. GS-Vertices außerhalb des Viewports sind erlaubt, sofern die geklippte
   Bounding-Box gültig ist. Der alte 0..2047-Vertex-Guard lehnte echte BIOS-
   Vertices mit negativen Koordinaten ab. Signierte Vertexgrenzen und
   64-Bit-Coverage-Rechnung schützen die CPU-Kantenberechnung weiterhin.
5. Der echte BIOS-Nebel verwendet (Cs-0)*As+Cd. Eine zweite GX-TEV-Stufe
   multipliziert gefiltertes RGB mit gefilterter Source-Alpha; ONE+ONE
   übernimmt die Addition. Source-FIX, klassische Source-over-Blends und
   Gouraud-MODULATE bleiben unterstützt. RGB vor dem Filtern mit variabler
   Alpha vorzumultiplizieren würde andere Kantenfarben erzeugen und wird
   dafür nicht gemacht.
6. Variierendes Z ohne Z-Schreibzugriffe darf eine konservative untere Grenze
   verwenden. Bei Z24/Z16 müssen sämtliche Vertexwerte in die Bitbreite passen;
   kein Wrap wird als monotone Tiefe ausgegeben. Jeder fehlgeschlagene
   konservative Z-Test führt zurück, nicht zu einem deaktivierten Z-Test.
7. Tiefen-/Aliasprüfung läuft vor dem Texture-Pack. Garantiert abgelehnte
   Legacy-Alpha-Draws stoppen beim ersten abweichenden Texel. Unsupported
   Source-over-Koeffizienten über 128 werden ebenfalls früh abgelehnt.

## Nachweise

- 4.160 unabhängige Alpha-Vergleiche für affine und projektive Texturierung.
- Verlinkter PPC: variable Alpha, Source-over, frühe Ablehnung mit zwei
  Sampling-Aufrufen, null Sampling bei Tiefenfehler, Q-/Gouraud-Übergabe,
  sessionale GX-Aktivierung ohne Änderung gespeicherter Einstellungen.
- Echte BIOS-Zeichenbefehle werden mit Raw-Textur und vollständigem VRAM
  aus einem kontrollierten Host-Kaltstart aufgenommen und auf dem
  tatsächlich verlinkten PPC-GX-Backend abgespielt. Alte 512-Zeilen-Bounds
  lehnen dieselben Pakete ab; die gesetzte 256-Zeilen-Grenze ermöglicht die
  sicheren Draws. Die Falltabelle im Prüf-ZIP nennt Übernahmen und exakte
  Alpha-Vergleiche, einschließlich großer Nebel-Dreiecke.
- Elf Host-Suites, 53 PPC-GS-Farb-/Tiefenspeicherfälle und der bestehende
  native FIX-Additiv-Test. DOL-Sektionen werden mit ELF verglichen; MEM1-
  Mindestreserve bestanden. Toolchain: devkitPPC r50/GCC16.1.0/libogc3.1.0.

GX-Befehle sind in PPC-Tests modelliert: weder echte EFB-Pixel noch Wii-FPS
sind dadurch nachgewiesen. Der Alpha-Schatten und die Z-/Coverage-Planung
laufen weiterhin auf der CPU. GX-Filter-/TEV-/Blend-Rundung kann von GS-RGB
abweichen. Der Build ist ein Hardware-Test, kein bestätigter Vollspeed-Fix.

Ergebnis des abgeschlossenen Replay: fünf von sechs erfassten BIOS-Paketen
übernommen, darunter zwei große Nebel-Dreiecke mit je 81.792 korrekten
Alpha-Metadaten. Insgesamt 165.666 echte BIOS-Alpha-Vergleiche bestanden;
zusätzlich 4.160 synthetische Vergleiche. Das kleine sechste Paket bleibt wegen
des Quelltexel-/Bounding-Box-Kostenlimits im Scalarpfad, ohne vorherige
Texturdekodierung. Diese sechs ausgewählten Fälle sind kein Übernahmeanteil
des gesamten BIOS-Laufs. Die neuen Zähler werden bei Kaltstart zurückgesetzt.

## Wii-Test und verbleibende Arbeit

Im Homebrew Channel Gekko2 R1389 GPU Test starten. Eigenes BIOS und bestehende
Memory-Card-/Game-Dateien behalten. Das Paket liefert keine Konfiguration,
keine BIOS-/Game-/Card-Dateien. Das aktuelle Log muss accepted>0 zeigen;
varying_alpha und additive_source_alpha machen die neuen Pfade sichtbar.
CPU_alpha_pixels misst verbleibende CPU-Metadatenarbeit. TIME_SAMPLE
GS_raster, GX_upload, GX_wait und die Übernahmezahlen gemeinsam bewerten.
Output-/Präsentations-Hz sind keine Gast-FPS.

Offen: echter Wii-Pixelvergleich, gemessener Speedup, Textur-Wiederverwendung,
Alpha-Schattenkosten, zusätzliche Blend-/Alpha-/Masken-/Fog-/Mip-/HIGHLIGHT-Zustände,
fehlende BIOS-Kacheln und ihre vorgelagerte Geometrie. Tekken, GT3 und der
R1386-BIOS-Crash werden hier nicht als behoben ausgewiesen.
IRQ-/VU1-Reset-/GIF-Q-/AOT-/JIT-/CHD-Fixes bleiben enthalten.

Öffentlicher Source enthält leere lokale AOT-Tabellen. Private BIOS-Profile,
VRAM-Pakete und BIOS-Datei gehören ausschließlich in getrennte Testdaten.
Build mit tools/build_r1389.sh und GEKKO2_NATIVE_GPU_TEST=1; optional den
privaten AOT-Profilpfad als Argument übergeben. Reproduktion: eigener BIOS-
Pfad an tools/capture_bios_gx_r1389.py; anschließend GEKKO2_BIOS_GX_CASES
auf das Ausgabe-Verzeichnis setzen und tools/verify_bios_native_r1389.py
mit ELF und --nm ausführen. Capture ist Host-Software, Replay modelliert GX.

Primärreferenzen:
https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS/Renderers/HW/GSRendererHW.cpp
https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS/Renderers/Vulkan/GSDeviceVK.cpp
https://github.com/devkitPro/libogc/blob/master/gc/ogc/gx.h
