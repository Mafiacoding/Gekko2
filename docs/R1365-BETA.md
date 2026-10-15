# Gekko2 R1365 – EE und Spiele-Controller Beta

Basis: R1364. Gebaut mit devkitPPC r50 / GCC 16.1.0 und libogc 3.1.0.

## Implementiert

- EE: Für bereits validierte, unveränderte native Rechenblöcke ohne Speicherzugriffe entfällt die erneute Analyse jeder einzelnen Instruktion. Die fünf besonderen BIOS-/SIF-PC-Grenzen werden einmal pro Block geprüft. Ereignis-, Interrupt-, Timer-, DMA-, Quellcodegeneration- und Budgetprüfungen bleiben erhalten. Keine Änderung der Gastzeit oder künstliche Erhöhung der EE-Zähler.
- SIO2: Direkter Controllerpfad unterstützt jetzt Konfiguration und Abfragen 0x40, 0x41, 0x43–0x47, 0x4C, 0x4D und 0x4F zusätzlich zu 0x42. Digital-, Analog- und native DualShock2-Antworten; native 21-Byte-Pakete liefern zwölf Druckwerte aus gedrückten Tasten (0/255). Teilübertragungen ändern nur tatsächlich empfangene Einstellungen. Verbindungsverlust und Initialisierung geprüft.
- R1364 Controller-Unterstützung, R1363 Sound-Beta, Lightrec-Pilot und bisherige DSI-Pfade bleiben Bestandteil des Builds. Diese Runde erweitert weder ARM noch DSI.

## Messung und Prüfung

100 warme Aufrufe pro Blockgröße, tatsächlich ausgeführter PPC-Code aus den verlinkten ELF-Dateien unter Unicorn. Identisches Testprogramm auf R1364 und R1365; vollständiger EE-Zustand und Gast-Instruktionszahl identisch.

| EE-Instruktionen pro Block | PPC-Instruktionen R1364 | PPC-Instruktionen R1365 | Weniger Host-Instruktionen |
|---|---:|---:|---:|
| 8 | 110,642 | 101,642 | 8.1 % |
| 16 | 137,042 | 108,042 | 21.2 % |
| 32 | 189,742 | 120,742 | 36.4 % |

Dies ist eine lokale Mikromessung, kein Wii-FPS- oder Bootzeitversprechen. Der Effekt hängt davon ab, wie oft das Spiel passende warme Rechenblöcke nutzt. 64 Instruktionen wurden vom vorhandenen Blockpfad nicht vollständig angenommen und sind nicht in der Tabelle enthalten.

Bestanden:

- 35 native Regressionstests: neue SIO2-Konfiguration, Controller, Sound, IPU, DMA, VU, Lightrec-Pipeline, Codecache, ISO und Checkpoints. Das bedeutet nicht vollständige Hardwareemulation.
- 80 tatsächlich ausgeführte PPC-Grenzfälle: BIOS-/SIF-Sonderadressen sowie 32-Bit-Wrap; Resultate und vollständige EE-Zustände identisch mit R1364.
- 12 PPC-Präfixfälle mit vollständigem EE-/Timer-/IRQ-/Display-Zustandsvergleich, Cachevollstand und anschließendem RAM-Store.
- 26 EE/IOP/Timer-Schedulervergleiche gegen das exakte 8:1-Interleave und sechs gemeinsame Mailboxfälle.
- 1.020 PPC-/native Controller-Vergleiche.
- DOL-Sektionen bytegleich zur ELF; Wii-Startmarkierungen geprüft.

Die Scheduler-Mikromessung zeigt weiterhin, dass Event-Batches allein nicht pauschal schneller sind. Keine neue Scheduler-Beschleunigung behauptet.

## Grenzen bei Spielen

Noch keine neue Messung auf einer echten Wii und kein neuer Tekken-Durchlauf. VU1-Geometrie, grauer Tekken-Hintergrund und ARM-Worker-Antworten sind mit dieser Änderung nicht als behoben nachgewiesen.

Der neue Druckmodus gilt für direkte SIO2-Kommandos. Die HLE-PADMAN/XPADMAN-RPC-Pfade liefern weiterhin den bisherigen Digital-/Analogumfang und lehnen Druck-/Actuator-Dienste teilweise ab. Kein physisches Rumble; nur der Protokoll-Konfigurationszustand wird gespeichert. SIO2 bleibt ein Batchmodell, keine vollständige ACK-/Bit-Timing-Emulation; ein Gastcontrollerport. Controller-Konfigurationszustand ist noch nicht Bestandteil des Checkpointformats.

Weitere SPU2-Funktionen wie Reverb, Noise, PMON, Gaussian-Interpolation und bestimmte Audio-/HLE-Dienste sind nicht vollständig. Alle für beliebige PS2-Spiele benötigten Module sind deshalb noch nicht implementiert.

## Wii-Test

1. DOL als boot.dol im bisherigen Homebrew-Verzeichnis ersetzen; bisherige BIOS-/Spielpfade behalten. Cold Boot verwenden.
2. Für einen vergleichbaren BIOS-/Tekken-Lauf zunächst exakt die bisherigen Einstellungen behalten. Die EE-Änderung greift bei aktivem EE-JIT/Native Blocks, „CPU inactive service guards“ und „EE native event windows (beta)“. „EE long blocks (32 instructions)“ ermöglicht die längere getestete Blockgröße. Kein neuer Schalter erforderlich.
3. R1364 und R1365 mit identischen Einstellungen bis zur gleichen Bildschirmphase vergleichen: reale Dauer, Guest-VBlank/FPS, EE/s, Native-Window-Zähler und Profilanteile. Keine Optionsänderung mitten im Vergleich.
4. Tekken: Analogmodus-Einrichtung, Insert-Coin/Start und Charakterwahl testen. Bitte Log mit Buildkennung R1365 und verwendete Einstellungen sichern. Ein gutes SIO2-Testresultat ersetzt keinen vollständigen Spielelauf.
5. Sound zunächst wie bisher lassen; für einen separaten Soundtest die vorhandene Sound-Beta aktivieren. GamePad weiterhin nur mit passender gepatchter Wii-VC-Passthrough-Firmware.

## Primärquellen für den Controllerpfad

- [PCSX2 PadDualshock2.cpp](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/SIO/Pad/PadDualshock2.cpp)
- [PCSX2 PadTypes.h](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/SIO/Pad/PadTypes.h)
- [ps2sdk padman padCmds.c](https://github.com/ps2dev/ps2sdk/blob/master/iop/input/padman/src/padCmds.c)

Quellen beschreiben Protokollwerte; keine unveränderte Übernahme eines PCSX2-Controller-Moduls.
