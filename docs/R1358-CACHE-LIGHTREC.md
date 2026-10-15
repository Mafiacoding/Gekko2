# R1358-A / R1358-B – Cache und Lightrec-Pilot

Stand: 10. Oktober 2026. Basis: R1357. Physische Wii-Ausführung und FPS-Gewinn
wurden hier nicht gemessen. Die vorhandenen R1357-DMA-, VU-IOR-, Pad- und
Checkpoint-Korrekturen bleiben erhalten.

## Builds

| Build | Änderung | Zweck |
|---|---|---|
| R1358-A | Cache-Eintrittsprüfung wiederverwenden, 16-Sektor-ISO-Cache, Diagnose | Nächster Wii-Vergleich gegen R1357 |
| R1358-B | A plus tatsächlich gelinktes Lightrec/GNU Lightning für IOP MULT/MULTU/DIV/DIVU | Begrenzter Backend-Pilot; kein zugesicherter Speedup |

B ersetzt ausschließlich diese registerbasierten Rechenkörper im skalaren
IOP-Ausführungspfad. Der bisherige Core führt weiterhin Fetch, PC-Fortschaltung,
HLE, Load-/Branch-Delays, Timer und Interrupts aus. IOP-Blockpfade können ihre
vorhandenen PPC-Körper weiterhin benutzen. B ist kein vollständiger Lightrec-
IOP-Backend und ersetzt den EE nicht. Ein separater Schalter wurde nicht nötig:
das bestehende **IOP dynarec** aktiviert diesen Pilot in B. Zusätzlich muss die
**6 MiB code arena** eingeschaltet sein. Nicht passende oder fehlgeschlagene
Ausführungen verwenden den bisherigen Pfad.

Lightrec-Code bekommt maximal 512 KiB aus der gemeinsamen MEM1-Codearena;
bei fehlendem Platz entsteht kein zusätzlicher ausführbarer Heap-Puffer.
Bibliotheksmetadaten verwenden den normalen Heap. Das Kopieren der Register
pro Pilot-Aufruf kann gegenüber den bestehenden PPC-Körpern langsamer sein.
B dient zur Integration und Korrektheitsprüfung, nicht als Behauptung eines
schnelleren vollständigen IOP. Die Bibliothek kompiliert synchron; kein
zusätzlicher Compiler-Thread.

## Was im bisherigen Code tatsächlich vorhanden war

R1357 reserviert bereits **6 MiB** für native EE-, IOP- und VU-Codebesitzer.
Die Linkerkarte zeigt `0x805921C0–0x80B921BF` vollständig in MEM1. Verdrängung
ist begrenzt und schützt aktive native Besitzer. Guarded successor links sind
vorhanden; ein Sprung zu einem Nachfolger prüft Identität, Quellgeneration,
Mapping und Ausführungsbudget. Es ist kein unbedingter ungeprüfter A→B-Patch.
`ppc_dynarec_finalize` benutzt bereits `DCFlushRange` und `ICInvalidateRange`.
Zusätzliche pauschale Cache-Flushes sind daher keine belegte Reparatur.

R1358-A: Arena `0x80592DA0–0x80B92D9F`.
R1358-B: Arena `0x805F8400–0x80BF83FF`.
Beide liegen weiterhin vollständig in MEM1; jeweils 6 MiB, Endadressen inklusiv.

Der Einbruch bei einer bestimmten EE-Zahl allein beweist weder einen Memory
Leak noch Cache-Thrashing. Die neue Logzeile `CODE_ARENA_LAYOUT` enthält den
Speicherbereich, freie Bytes, größte freie Nutzlast, freie Bereiche, lebende
Besitzer, Allokationen/Freigaben, Suchschritte und Wiederverwendungen der
Eintrittsprüfung. Trimm-Freigaben zählen nicht als freigegebene Besitzer.
`BLOCK_CACHE` und `EE_CACHE_PRESSURE` enthalten weiterhin Wiederübersetzung,
Kollisionen, Stichprobenzeit und Verdrängung.

## Konkrete Verbesserungen

Die Prüfung auf einen passenden freien Bereich wird mit Größe und
Arena-Generation zwischengespeichert. Der anschließende Compilerabschluss
verwendet dieses Ergebnis, solange keine Allokation, Freigabe oder Verkleinerung
die Arena geändert hat. Eine fehlgeschlagene Eintrittsprüfung wird nach einer
Freigabe ebenfalls ungültig. Besitzer und relative Sprünge bewegen sich nicht.

Der ISO-Leser prüfte die Dateigröße bei **jedem Sektor** mit zusätzlichen
Seek-Aufrufen. Bei normal geöffneten Images wird sie nun beim Öffnen bestimmt.
Ein begrenzter Cache liest bis zu 16 benachbarte Sektoren zusammen und speichert
höchstens 37.632 Rohdatenbytes pro geöffnetem Image, zuzüglich kleiner Metadaten.
2048-ISO und 2352-Mode-1/Mode-2 bleiben unverändert dekodiert. Geliehene
Verzeichnisansichten teilen den Cache; Unmount gibt ihn frei. Kurzreads und
LBA-Grenzen werden geprüft. Bei fehlendem Cache-Speicher bleibt der ursprüngliche
Reader verfügbar. Geöffnete Images gelten bis Unmount als unveränderlich.
Keine komplette Ramdisk und kein Hintergrund-Thread wurden hinzugefügt.

## Prüfung

- **19 native Regressionstests bestanden**, darunter die bisherigen R1357-
  Geometrie-, DMA-, Pad- und CDROM-Checkpoint-Tests.
- **100.000 zufällige Arena-Allokationen/Freigaben/Trims** erhalten die Daten
  lebender Besitzer und ergeben am Ende wieder einen freien zusammenhängenden
  Bereich. Eintrittsprüfung, Generationen und Zähler sind separat geprüft.
- ISO/Mode-1/Mode-2: Nutzdaten stimmen mit dem direkten Leser überein; ein
  wiederholter Test hat 49 von 50 Anfragen aus dem Cache bedient. Letzter
  Sektor, ungültige LBA, Verzeichnisansichten und Unmount geprüft.
- **16.384 echte native Lightrec-Ausführungen** mit wechselnden Registern und
  Cache-Kollisionen: signierte/unsignierte Multiplikation und Division,
  Division durch null, INT_MIN/-1 und unveränderte übrige CPU-Zustände.
- Lightrec im realen IOP-Core: pending LW wird erst nach dem MULT sichtbar,
  MULT im Branch-Delay-Slot erhält das echte Sprungziel, genau ein Tick und
  eine Instruktion pro Rechenkörper. Geänderte Opcodes erzeugen keinen alten
  Cache-Treffer. Der vorhandene Interrupt-/Exception-Pipelinetest besteht.
- Tekken-Checkpoint: von **2.255.916.427 auf 2.287.915.507 EE-Instruktionen**
  weitergelaufen, EE/IOP nicht angehalten. Charakterauswahl weiterhin sichtbar.
  ISO: 39 Anfragen, 32 Cache-Hits, 7 Fills. In diesem Ausschnitt **0 geeignete
  Lightrec-Rechenkörper**; deshalb kein Spiel-Speedup daraus ableitbar.
- A und B mit **devkitPPC r50 / GCC 16.1.0 / libogc 3.1.0** gebaut.
  DOL-Sektionen, Einsprungadresse, ELF-Übereinstimmung und Wii-Startmarker geprüft.
  Das ersetzt keinen Boot-Test auf physischer Wii oder Dolphin.

Das beigefügte Bild ist ein tatsächlicher **nativer Software-Scanout**,
640×224 Pixel, kein Wii-Foto. Die 3D-Figuren fehlen weiterhin. Es ist kein
Nachweis dafür, dass das Charaktermenü auf der Wii bereits erreicht wurde.

## Grafik, MMI und IOS80

Der GX-Pfad konvertiert GS-Pixel in gekacheltes **GX_TF_RGBA8**, einschließlich
4×4-Kacheln und AR/GB-Planes. Geeignete Sprites, Flächen und Dreiecke können
auf GX laufen; andere GS-Zustände bleiben im CPU-Fallback. TPL ist hierfür
kein notwendiger Laufzeitcontainer. Rohes PS2-VRAM ist wegen Layout und
Pixelformaten keine gültige GX-RGB565-Textur.

EE-MMI wird je nach Opcode in mehrere PPC-Operationen zerlegt: 32-Bit-Lanes,
16-/8-Bit-Elemente, Shuffle/Pack und Sättigung. 64-Bit-Arithmetik benötigt
Wortpaare und Carry. Nicht unterstützte Fälle verwenden vorhandene Helfer.
Lightrec unterstützt diesen R5900-/VU-Befehlssatz nicht direkt.

IOS80 ist das IOS des Systemmenüs, kein belegter CPU-Takt-Schalter für eine
normale Wii. Der dokumentierte `c2w_patcher`-Taktanstieg auf 1,215 GHz gehört
zur **Wii U** und ihrer cafe2wii-Umgebung. Kein IOS80-Wechsel, keine
Systeminstallation und kein vermeintlicher aggressiver L2-Modus implementiert.
VU-Berechnungen, DMA, GS-Speicheränderungen und Audio-Erfolgsmeldungen werden
nicht verworfen oder erfunden. Aus 0,17 FPS folgt keine GS-Wartezeit von 99 %.

## Nächster Wii-Test

1. Zuerst **A** mit denselben Einstellungen wie R1357 und einem Cold Boot.
2. Gleiche BIOS-/Tekken-Phase und reale Laufzeit vergleichen; möglichst Log
   vor und nach der bisher langsamen Phase sichern.
3. B separat mit **IOP dynarec** und **6 MiB code arena** an testen.
   `LIGHTREC_IOP compiled=1` bestätigt den B-Build; `bodies` zeigt tatsächliche Aufrufe. Null bedeutet, dass
   dieser Pilot nicht ausgeführt wurde; `ready=1` allein beweist kein Tempo.
4. Für START weiterhin **MINUS+A**, für CROSS **A**. PLUS pausiert die Sitzung.
   Cold Boot setzt die Emulation neu auf. ARM/IOS-Anlauf wie bisher, kein IOS80.

## Reproduzierbarer Build

```sh
sh tools/build_r1358.sh 0  # A
sh tools/build_r1358.sh 1  # B
python3 tools/verify_r1358_native.py
python3 tools/verify_lightrec_r1358.py
```

`DEVKITPRO` und `DEVKITPPC` müssen auf die installierte moderne SDK zeigen.
Der Quellcode enthält alle gepinnten Lightrec-/Lightning-Quellen und Lizenzen.
BIOS, Spielimage und RAM-Checkpoints sind nicht im öffentlichen Quellarchiv.

Referenzen:
- https://github.com/pcercuei/lightrec
- https://github.com/emukidid/pcsxgc/tree/a30b271718541dce8a8fe161b56b90553f66ab7d/pcsx_rearmed/deps/lightning
- https://wiibrew.org/wiki/IOS80
- https://github.com/FIX94/c2w_patcher
