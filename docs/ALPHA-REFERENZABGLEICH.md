# PCSX2-/Play!-Abgleich für die Alpha-Planung

**Historischer Audit vor der Umsetzung.** Die konkreten Menü-/CLUT-/Scanout-/
Karten-/IPU-Korrekturen sind in [BETA12.28](BETA12.28-ALPHA-CORE.md) und
[ALPHA-UMSETZUNG](ALPHA-UMSETZUNG.md) dokumentiert. Die damaligen Messwerte
beschreiben BETA12.27; der aktuelle Optionsprobe respektiert All-OFF.

17.10.2026. Gekko2-Codebasis `c51ab567` (BETA12.27). Read-only-Audit,
keine Übernahme eines fremden CPU-/Renderer-Backends. Upstream ist eine
Emulatorreferenz, keine automatisch fehlerfreie PS2-Hardware-Spezifikation.

## Fixierte öffentliche Quellen

Per `git ls-remote <repo> HEAD` geprüft und die unten genannten Dateien
über TLS von raw.githubusercontent.com gelesen:

- PCSX2: **`091ab68708a555198d80a7216965481c7ddd0d98`**.
- Play!: **`d44bfa1814b729d4a3ed3c6c3a945432d5ee5bbe`**.

PCSX2-Dateien tragen SPDX GPL-3.0+; Play! nennt in `License.txt` die
BSD-Bedingungen. Bei späterer Codeübernahme passende Notices und Projektlizenz
prüfen. Hier werden nur Befunde, Testspezifikationen und Quelllinks hinzugefügt.
Die Referenzdateien selbst liegen außerhalb des Checkouts und werden nicht
als neuer Vendoring-Import committed.
Datei-URLs und SHA256 stehen im
[Prüfprotokoll](verification/ALPHA-AUDIT-2026-10-17.json).

| Referenz | Gelesene Teile / Zweck |
|---|---|
| [PCSX2 GSClut.cpp](https://github.com/PCSX2/pcsx2/blob/091ab68708a555198d80a7216965481c7ddd0d98/pcsx2/GS/GSClut.cpp), [GSClut.h](https://github.com/PCSX2/pcsx2/blob/091ab68708a555198d80a7216965481c7ddd0d98/pcsx2/GS/GSClut.h) | `CanLoadCLUT`, `WriteCLUT32/16/16S_CSM2`, CSM1-Loader, `Read32`: gemeinsame Halbwörter, CSA/CLD und Neuinterpretation |
| [PCSX2 GSLocalMemory.cpp](https://github.com/PCSX2/pcsx2/blob/091ab68708a555198d80a7216965481c7ddd0d98/pcsx2/GS/GSLocalMemory.cpp) | PSM-Dispatch und lokale Transfers als Vergleichsstellen für unabhängige Testvektoren |
| [PCSX2 GSState.cpp](https://github.com/PCSX2/pcsx2/blob/091ab68708a555198d80a7216965481c7ddd0d98/pcsx2/GS/GSState.cpp) | TEX-/CLUT-Updates, Register-/Displaypfade als Folgeaudit-Einstiegspunkte |
| [PCSX2 Gif.cpp](https://github.com/PCSX2/pcsx2/blob/091ab68708a555198d80a7216965481c7ddd0d98/pcsx2/Gif.cpp) | GIF-Steuer-/DMApfade; tatsächlichen SIGNAL-Konsumenten separat beachten |
| [PCSX2 IPU.cpp](https://github.com/PCSX2/pcsx2/blob/091ab68708a555198d80a7216965481c7ddd0d98/pcsx2/IPU/IPU.cpp) | Command-/BUSY-/Reset-/Workerstruktur, kein kompletter Decoderkonformitätstest |
| [PCSX2 MemoryCardProtocol.cpp](https://github.com/PCSX2/pcsx2/blob/091ab68708a555198d80a7216965481c7ddd0d98/pcsx2/SIO/Memcard/MemoryCardProtocol.cpp) | GetSpecs, SetSector, Terminator, ReadData, WriteData und EraseBlock; PS1-/PS2-Trennung |
| [Play! GSHandler.cpp](https://github.com/jpd002/Play-/blob/d44bfa1814b729d4a3ed3c6c3a945432d5ee5bbe/Source/gs/GSHandler.cpp) | `GetDisplayRect`, `GetCurrentDisplayInfo`: separate Kreise, Offsets, Feld-/Framehöhe und Layer |
| [Play! Vif.cpp](https://github.com/jpd002/Play-/blob/d44bfa1814b729d4a3ed3c6c3a945432d5ee5bbe/Source/ee/Vif.cpp) | VIF-/VU-Interlocks als zweite Referenz neben vorhandenen PCSX2-Quellen |
| [Play! IPU.cpp](https://github.com/jpd002/Play-/blob/d44bfa1814b729d4a3ed3c6c3a945432d5ee5bbe/Source/ee/IPU.cpp) | BDEC/IDEC und fortsetzbare Commandverarbeitung als Folgeaudit-Einstieg |
| [Play! Iop_McServ.cpp](https://github.com/jpd002/Play-/blob/d44bfa1814b729d4a3ed3c6c3a945432d5ee5bbe/Source/iop/Iop_McServ.cpp) | Dateisystem-/MCSERV-Schicht; nicht mit niedrigem SIO2-Wireprotokoll verwechseln |

„Gelesene Teile“ bezeichnet die konkret untersuchten Funktionen und
Suchtreffer, nicht einen vollständig ausgeführten Upstream-Konformitätstest.
Die bereits gebündelten PCSX2-VU-/VIF-Referenzen und die früheren gepinnten
IPU-Berichte bleiben zusätzliche Referenzen; Revisionen nicht vermischen.

## Nachweisbare Unterschiede und noch offene Hypothesen

### 1. CLUT: echtes Modellierungsdefizit, BIOS-Ursache offen

`source/hw/gif.c:gs_load_clut/gs_sample_clut` hält logische CT32-/CT16-Werte
in einem `uint32_t`-Cache. Das unterstützt dokumentiert nur gleichformatige
Palettenreuse. PCSX2 speichert CT32-Teile in gemeinsamen Halbwörtern und liest
sie bei CPSM-/CSM-Wechseln anders aus. Ein Formatwechsel ohne neues Laden
kann dadurch bei Gekko2 andere Daten sehen. Die fehlende Reinterpretation ist
im eigenen Quellkommentar bereits ausdrücklich vermerkt.

PCSX2s `CanLoadCLUT` und Gekko2 haben für CLD 0..5 grundsätzlich dieselbe
Load-/CBP-Vergleichsstruktur. Es gibt keinen Beleg, dass pauschal häufigeres
Reloaden richtig wäre; CLD=0 muss gespeicherte Palette erhalten.
CSM2-CSA-Bankretention wurde bereits in BETA12.22 korrigiert und darf nicht
unter dem Vorwand „Palette neu implementieren“ verloren gehen.

Folgetest A04: CT32 laden → auf CT16/16S ohne Reload umschalten → definierte
Indizes lesen; umgekehrt, alle CSA-Ränder, CSM-Wechsel, CLD/CBP0/1 und TEX2.
Die Erwartung aus separatem Halbwortmodell und dokumentiertem Verhalten ableiten.
Erst ein BIOS-Draw mit derselben Folge verbindet den Defekt mit seinen Kacheln.

### 2. Scanout: Einzelkreisauswahl statt vollständiger Zusammenführung

`source/main.c` wählt `en1 ? dispfb1 : dispfb2`; beide Kreise gleichzeitig
werden nicht nach vollständiger PMODE-Blendregel zusammengeführt.
`gs_decode_display_region` verarbeitet Quellursprung, MAG und Feldhöhe,
aber nicht den kompletten relativen PCRTC-Layeraufbau.
Play! stellt getrennte Layer und DISPLAY-Offsets dar; auch dessen gezeigte
Assertions/Sonderfälle sind keine allgemeine Hardwarewahrheit.

A05 braucht eigens erzeugte Zweikreis-/BGCOLOR-/Offsetbilder und exakte
PMODE-Regeln. Korrekte VRAM bei falscher Ausgabe würde diesen Bereich
belasten; fehlende Geometrie schon in VRAM würde ihn entlasten.
Kein Nachweis, dass dieser Unterschied die aktuelle SCPH-50004-Szene betrifft.

### 3. PS2-Speicherkarten: fehlendes natives Gerät und Teil-HLE

`include/core/hw/iop_sio2.h` und `source/hw/iop_sio2.c` spezifizieren
ausdrücklich PS1-Read/Write/GetID mit 128-Byte-Sektoren/128-KiB-Karte.
PS2-Probe/Erase/GetSpec/ECC-Protokoll ist dort ausdrücklich ausgeschlossen.
`ee_core.c` behandelt MCSERV GetInfo cardless und OPEN mit fehlendem Eintrag;
weitere Befehle benutzen Legacy-HLE-Antworten. Das ist keine vollständige
native PS2-Speicherkartenunterstützung.

PCSX2 trennt PS1-Zustand von PS2 GetSpecs/SetSector/Read/Write/Erase und
Terminatorantworten. Play! hat eine eigene MCSERV-Dateischicht.
A14 muss beide Ebenen kohärent implementieren und ehrlich Fehler liefern.
GT3s Fragen passen als Untersuchungspunkt, beweisen aber nicht die alleinige Ursache.

### 4. IPU: vorhandener Decoder mit fehlender breiter Konformität

Gekko2 verarbeitet die gültigen Befehle 0..9 und hat FIFO-/Command-Phasen,
VLC, Koeffizienten, IDCT, CSC und PACK. `unimplemented_commands` für Opcode
15 ist kein Beleg, dass IDEC fehlt. Reservierte Opcodes nicht als fehlende
legale Funktionen zählen. Die frühere IDEC-/DMA-Korrektur und deren Tests
sind in `IPU-ACCURACY-CONTINUATION.md` beschrieben.

Der weitere Bedarf liegt bei Streamvarianten, Fehlerfällen, Reset/Starvation,
DMA-Reihenfolge und Gastintegration. Ein PCSX2-/Play!-Command-Switch alleine
belegt weder Pixelrichtigkeit noch Filmfortschritt. Hardware-Latenzen brauchen
zusätzliche Evidenz. IPU-Aktivität in Tekken kann relevant sein; in der früher
analysierten Crash-Sitzung wurde kein IDEC/BDEC/CSC/PACK beobachtet.

### 5. JIT und Wii: Architektur nicht von x86/SIMD übernehmen

Gekko2s PPC-Emitter, Zulassung und Helperverträge selbst prüfen. QFSRV,
MADD/MADDU und ihre Pipeline-1-Varianten existieren trotz alter gegenteiliger
Kommentare. Ein einfacher TODO-/„unimplemented“-Scan ist deshalb ungeeignet,
die restlichen legalen Opcodes oder Prozente zu bestimmen.

EE-/IOP-/VU-Decoder-, Interpreter-, Single-, Block-, Ketten- und AOT-Abdeckung
getrennt inventarisieren. PCSX2-x86-Register-/SIMD-Allokation passt nicht auf
zwölf PPC-Lanes. Die bereits gepinnte
[nullDC4Wii-Review](NULLDC4WII-REVIEW-R1304.md) erklärt brauchbare Konzepte:
Helperbarrieren, pinned Register, Codevalidierung und bounded Dispatch.
SH4-/Dreamcast-Kompatibilität oder Speedwerte sind nicht übertragbar.

## Was dieser Audit nicht belegt

Keine aktuelle SCPH-50004-/Crash-/Tekken-/GT3-Ausführung, keine tatsächlichen
Wii-EFB-Pixel, kein neuer PPS/FPS-Wert, keine universelle Liste „alles fehlt“.
Die priorisierten Implementierungen und ihre Abnahmekriterien stehen im
[Alpha-Arbeitsplan](ALPHA-ARBEITSPLAN.md); vorhandene Tests und öffentliche
Quellen reichen zum Start der Batches, nicht zum Abschließen der Spielsupport-Gates.
