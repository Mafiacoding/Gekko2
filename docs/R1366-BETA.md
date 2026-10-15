# Gekko2 R1366 – IOP- und EE-Entlastung

Basis R1365; devkitPPC r50 / GCC 16.1.0, libogc 3.1.0.

## Änderungen

**IOP:** Der Dispatcher liest warme Blöcke nicht mehr vollständig vorab. Jeder erzeugte PPC-Slot ruft bereits vor seinem nativen Körper `iop_core_block_prepare` auf. Dort laufen die bisherigen Tick-/HLE-Dienste, der tatsächliche Instruktions-Fetch und der Vergleich von PC und Opcode. Erst danach dürfen native Instruktion und Register-Residency-Refresh laufen. Die vorgelagerte doppelte Prüfung war deshalb entbehrlich.

Bei geänderten Instruktionen führt der vorhandene Recoverypfad die tatsächlich geladene Instruktion aus und zählt/retired genau einmal. Der alte Owner wird danach freigegeben; diese Invalidierung erhöht jetzt auch die Cache-Stale-Statistik. Kalte Blöcke und Blockwachstum lesen weiterhin den Code zur Übersetzung. Keine ungeprüfte Ausführung aus einem fremden oder veränderten Block.

**EE:** Der seit R1365 einmalige Grenztest für vertrauenswürdige, unveränderte Rechenblöcke benötigt kein temporäres Array der fünf Sonder-PCs mehr. Wortausrichtung wird einmal geprüft; fünf direkte unsigned Bereichstests erhalten die bisherigen Grenzen einschließlich 32-Bit-Adressüberlauf.

Keine zusätzlichen Gastinstruktionen, kein schneller gestellter Gasttimer, keine Änderung des EE:IOP-Verhältnisses, der IRQ-Reihenfolge oder der unterstützten Opcodes. Der bestehende Scheduler und seine Synchronisationsbarrieren bleiben erhalten. R1365 Spiele-Controller-Erweiterung und vorherige Sound-/Controller-Betas bleiben enthalten.

## Lokale Messung

100 Aufrufe pro Fall, tatsächlich ausgeführter PPC-Code aus der jeweiligen ELF unter Unicorn. Vollständige Zustands-Hashes sind im jeweiligen R1365/R1366-Vergleich identisch. Gezählt werden Hostinstruktionen, keine gemessenen Wii-Zeitintervalle.

### IOP: warmer Owner mit acht Instruktionen

| Budget pro Aufruf | PPC-Instruktionen R1365 | PPC-Instruktionen R1366 | Weniger Hostinstruktionen |
|---|---:|---:|---:|
| 1 | 118,700 | 88,200 | 25.7 % |
| 2 | 182,800 | 152,300 | 16.7 % |
| 4 | 311,000 | 280,500 | 9.8 % |
| 8 | 567,200 | 536,700 | 5.4 % |

Der große Gewinn bei kurzen Budgets entsteht, weil R1365 auch die sieben nicht ausgeführten Wörter des längeren Owners erneut las. Bei einem Owner mit nur **einer** Instruktion beträgt der Gewinn **5,7 %**: 93.416 statt 88.116 Hostinstruktionen. Das ist der passendere Vergleich für dauerhaft kurze IOP-Blöcke.

### EE: warmer reiner Rechenblock

| EE-Instruktionen pro Block | PPC-Instruktionen R1365 | PPC-Instruktionen R1366 | Weniger Hostinstruktionen |
|---|---:|---:|---:|
| 8 | 101,642 | 95,142 | 6.4 % |
| 16 | 108,042 | 101,542 | 6.0 % |
| 32 | 120,742 | 114,242 | 5.4 % |

Die beiden Einsparungen sind verschiedene Pfade und dürfen nicht als gemeinsamer FPS-Prozentsatz addiert werden. Kein neuer Lauf auf echter Wii, kein bestätigter Tekken-/BIOS-Bootzeitgewinn und keine Zusage einer Verdopplung. Ein nennenswerter Gesamteffekt hängt vom tatsächlichen Anteil dieser Pfade ab.

## Prüfungen

- 35 native Regressionstests bestanden: Controller/SIO2, SPU2 und Sound-Checkpoints, IPU, VU, DMA, Lightrec-Pipeline, Codecache und ISO.
- 48 PPC-/Interpreter-Vergleiche mit warmen Ownern: geänderte Instruktion an jeder der acht Positionen, Budgets 1/3/8, Änderungen an nicht ausgeführten Enden und Syscall-Recovery. Nach kurzem und anschließend langem Grant jeweils identischer vollständiger IOP-Zustand und kontrollierter RAM-Bereich.
- 477 linked-PPC IOP-Programme gegen den Interpreter: ALU, HI/LO, Speicherformen und Aliase, Branch-/Load-Delay, Ausnahmen, IRQ/Idle, HLE-Grenzen, Quellcodeänderungen und Allokationsfehler. EE8/IOP1-Reihenfolge über Profilierungsintervalle geprüft.
- 21 Zustände bei IOP-Owner-Wachstum 1→2→4→8, gemischten Budgets und Codeänderung.
- 80 EE-Sonderadress-/Wrap-Fälle identisch mit R1365; vollständige EE-Zustände identisch.
- 12 EE-Präfixfälle einschließlich RAM-Store, Cachevollstand und Timer-/IRQ-/Displayvergleich.
- 26 vollständige EE/IOP/Timer-Schedulervergleiche und sechs gemeinsame Mailboxfälle. Event-Batches bleiben im Test nicht pauschal schneller; keine neue Scheduler-Beschleunigung behauptet.
- DOL-Sektionen bytegleich zur finalen ELF, Wii-Startmarkierungen geprüft.

Plattform-Allokation und Cachemaintenance sind in den PPC-Tests modelliert. Bluetooth, echte USB-Geräte, ARM-Firmware, physisches Wii-Timing und DSI-Hardwareexceptions werden dadurch nicht validiert.

## Optionen und späterer Wii-Test

Kein neuer Schalter erforderlich. IOP-Entlastung greift bei **IOP dynarec**. EE-Entlastung braucht weiterhin EE-JIT/native Blöcke, **CPU inactive service guards** und **EE native event windows (beta)**. Längere EE-Blöcke nutzen die vorhandene Long-Blocks-Option.

Für den ersten Vergleich R1365 und R1366 mit identischen Einstellungen und Cold Boot starten. Dauer bis zum selben BIOS-/Tekken-Bild, EE/s, Guest-VBlank/FPS und Profilanteile vergleichen. Nicht alle Optionen gleichzeitig ändern. Du musst den Test nicht während der Arbeit durchführen.

## Noch offen

Diese Runde behebt keine nachgewiesene ARM-Antwortlücke und ergänzt keine neuen ARM-/IOS-Funktionen. VU1-Geometrie und grauer Tekken-Hintergrund sind noch nicht als behoben nachgewiesen. Weitere Sound-/HLE-/Spielemodule fehlen weiterhin; vollständige Kompatibilität aller Spiele ist nicht erreicht. Details der übernommenen Controller-/Sound-Betas stehen in den R1365-/R1364-Dokumenten des Quellcodes.
