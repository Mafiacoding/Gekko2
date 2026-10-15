# R1386: Fix-Abgleich, 12. Oktober 2026

Der aktuelle Quellstand enthält die unten aufgeführten Änderungen aus dem
verfügbaren Gesprächskontext, den abgerufenen Verlaufstreffern und den
versionierten R1378–R1385-Handoffs. Der ältere Chatlink ließ sich nicht
vollständig abrufen. Verlaufssuche liefert einzelne Treffer und ist kein
vollständiger Chat-Export; eine lückenlose Prüfung unsichtbarer Nachrichten
ist deshalb nicht behauptet.

| Änderung | Im aktuellen Quellstand | Nachweis |
| --- | --- | --- |
| Moderne Docker-Toolchain für DSI | Ja: devkitPPC r50/GCC 16.1.0, libogc 3.1.0 | Erfolgreicher Wii-Crossbuild; R1384-Toolchain-Dokument |
| Q-/FDIV-Sticky-Statusfix R1380/R1382 | Ja, in gemeinsamer VU-Pipeline | Q-Pipeline- und Hardware-Tests |
| FMAC-Flag-Latenz und FSSET-/FDIV-Reihenfolge | Ja | Hardware-Test und linked timed VU |
| GIF_STAT APATH/P3Q/P2Q/P1Q | Ja, Bits 10–11/6/7/8 | gif.c und Hardware-Test |
| Verschachtelter GIF-Pfadstatus | Ja, aktiver Pfad wird wiederhergestellt | gif.c, R1384 |
| VU0-Hostlimit ist kein regulärer Stop | Ja, RUN/TPC bleiben erhalten | VU-Mikro- und Hardware-Test |
| VU0-M-Fortsetzung und COP2-Synchronisierung | Ja | Hardware-Test, VU0/COP2-Test |
| Q/P, WAITQ/WAITP und EFU | Ja | VU-Mikro- und linked hybrid Tests |
| 128-Bit-Registerhaltung | Ja, einschließlich oberer EE-Lanes | R1384-Allocator und COP2/MMI-Regressionen |
| Zeitgesteuerte VU0/VU1-Hybrid-JIT-Blöcke | Ja; R1385 bleibt erhalten | Linked production PPC VU-Test |
| Getaktetes XGKICK, Ring-Wrap und Live-Quelle | Ja | XGKICK-Timing- und Grenztests |
| Unvollständiges PATH1-Paket vergiftet Folgepaket nicht | Ja | XGKICK-Grenztest |
| GIF/VIF/DMA-Stalls und MFIFO | Ja | Hardware-, VIF- und MFIFO-Tests |
| VIF-Fill-Modus und Register-MMIO | Ja | VIF-Transporttest |
| DATE und Texturkoordinaten | Ja | DATE-Regression |
| CHD/CD-Sektorabbildung und Hunk-Cache | Ja | CHD-Regression mit synthetischen Fixtures |
| AOT-Delay-Slots und BC2-Likely | Ja | Linked BC2 und Host-BC2 |
| LQC2/VF0 und vollständige Adressspanne | Ja | R1379-AOT3-Code und COP2-Regressionen |
| J/JAL-Adresse verwendet obere Bits von PC+4 | Ja | Sechs linked PPC Sprung-Orakel |
| DSI-Backpatch/Fallback und Registererhalt | Ja, bestehender R1351-Pfad | 28 linked PPC DSI-Zustandsvergleiche; kein physischer Wii-Lauf |
| IRQ-iWakeupThread wechselt nicht mitten im Callback | Ja, R1386 | Host-Test und drei linked PPC Kontextfälle |
| IRQ-Präemption bei ausgeschaltetem IE verweigern | Ja, R1386 | Dieselben IRQ-Tests |
| VU1-Kaltstart-Reset | Ja, R1386 | Cold-reset-Test |
| Controller-Protokoll beim Checkpoint erhalten | Wiederhergestellt, PADP-Block | Roundtrip und Legacy-DMA-ID-Test |
| Gecachter AOT-Lookup für Treffer und Fehlschläge | Ja, 128 Einträge mit vollständigem PC | Messung 128 warm misses: 5504 PPC-Instruktionen |
| Bewachte AOT-Rechenfenster | Ja; Live-Wörter und Ereignisgrenzen bleiben geprüft | 48 vollständige Zustandsvergleiche |
| Inaktive Grafikdienste im Scheduler überspringen | Ja; Prüfung am kanonischen Gerätezustand | Messung und Deadline-/DMA-Tests |
| Private AOT-Spielabdeckung | 256 statisch dekodierte Tekken-Blöcke plus 108 beobachtete BIOS-Blöcke | 88 linked Spielblock-Vergleiche; keine erfundenen Laufzeithäufigkeiten |

Die öffentliche Quelle enthält leere generierte AOT-Tabellen. Der ausgelieferte
Build enthält die privat generierten Tabellen. Die privaten Eingaben werden
separat gesichert und enthalten weder vollständiges BIOS noch Spielabbild.

## Weiterhin offen

- Der auf der Wii gemeldete BIOS-Crash ist nicht auf Hardware als behoben bestätigt.
- Tekken: Frühere Host-Läufe erreichten beide Charakterwahlen, VS und fehlerhafte
  3D-Ausgabe. Ein erneuter Lauf erreichte Titel und VS, anschließend einen
  Black-Screen. Die erneuten Läufe mit Tasteneingaben belegten keine vollständige
  Charakterwahl/Kampfszene; bei 460 Mio. IOP-Scheduler-Ticks blieb die Anzeige
  schwarz. Korrektes Kampf-Rendering und der Wii-Black-Screen sind offen.
- GT3: Im früheren Host-Lauf erschien der echte Copyright-Bildschirm; wiederholte
  Memory-Card-GetInfo-Abfragen verhinderten weiteren belegten Fortschritt.
  Kein Rennen und keine vollständige In-game-Validierung.
- Gemessene PPC-Instruktionszahlen sind keine Wii-FPS oder Broadway-Zyklen.
- Das vor dem Reset erzeugte Kampfprofil und die Spielstände sind nicht Teil
  des wiederhergestellten Quellstands. Statische Spiel-AOT ersetzt diese
  Profilabdeckung nur teilweise.
