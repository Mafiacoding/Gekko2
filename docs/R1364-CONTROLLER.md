# Gekko2 R1364 Controller + Sound Beta

R1364 basiert auf dem gesicherten R1363-Quellstand. Sie ergänzt eine gemeinsame Eingabeanbindung für die normalen Wii-/Wii-U-Pads. R1363-Sound, EE/JIT, Scheduler, GX, DSI und ARM-Worker bleiben grundsätzlich erhalten. Diese Version verspricht keinen FPS-Zuwachs.

## Eingabepfade

| Gerät | Anbindung in dieser Wii-DOL | Umfang / Voraussetzung |
| --- | --- | --- |
| GameCube-Pad / WaveBird | libogc PAD, alle vier Anschlüsse | Wii mit GameCube-Anschlüssen; C-Stick und analoge Trigger enthalten |
| Wii Remote / Wii Remote Plus | libogc WPAD, Kanäle 1–4 | Normale Tasten, aufrechte Orientierung |
| Wii Remote + Nunchuk | libogc WPAD | Linker Stick, C und Z |
| Classic Controller | libogc WPAD Classic | Zwei Sticks, L/R und ZL/ZR |
| Classic Controller Pro | libogc WPAD Classic | Zwei Sticks, L/R und ZL/ZR |
| Wii U Pro Controller | moderne libogc WPAD-Implementierung | Als Classic-Typ WIIU erkannt; beide Sticks und L3/R3 |
| Wii U GamePad | mitgelieferte MIT-libwiidrc von FIX94 | **Gepatchter Wii-VC-Start mit libwiidrc-Passthrough erforderlich**; normaler vWii-HBC allein genügt nicht |
| Nintendo GameCube-USB-Adapter / kompatibles WUP-028-Protokoll | neue asynchrone libogc-USB-Anbindung | Experimentell; VID 057e/PID 0337, ggf. Adapter auf Wii-U-Modus; alle vier Adapteranschlüsse gelesen |

Physische Lenkrad-/Griffhalterungen verwenden die eingelegte Wii Remote. Spezialgeräte wie Balance Board, Gitarren, Schlagzeug, beliebige USB-HID-Geräte und neue Funkprotokolle sind nicht vollständig abgedeckt. Motion-/IR-Daten, Touchscreen und Mikrofon werden nicht auf PS2-Eingaben umgesetzt.

Die Eingabe übernimmt **ein aktiver Controller für PS2-Port 1**. Damit sind vier Host-Kanäle verwendbar, aber noch kein neues PS2-Mehrspieler-/Multitap-System implementiert. Zum Wechseln den bisherigen Controller loslassen und am anderen eine Taste drücken oder einen Stick bewegen. Während einer gehaltenen Eingabe bleibt der aktive Controller ausgewählt. Zustände unterschiedlicher Geräte werden nicht zusammengemischt.

Beim Abziehen werden Tasten und Achsen freigegeben. Der virtuelle PS2-Pad-Anschluss bleibt gemäß bisherigem Verhalten verfügbar. Kurze Tastenimpulse bleiben bis zu einer tatsächlichen Gast-Abfrage gespeichert; beim Gerätewechsel wird der alte Impulspuffer verworfen.

## Belegung

Die Zuordnung ist eine Projektentscheidung, keine Hardware-Eigenschaft:

| Host-Eingabe | PS2 / Launcher |
| --- | --- |
| A, B, X, Y | Cross, Circle, Square, Triangle |
| D-Pad | PS2-D-Pad / Menü |
| Linker Stick | Linke PS2-Achsen; zusätzlich Menü-Navigation |
| Rechter Stick / C-Stick | Rechte PS2-Achsen |
| L/R | L1/R1 |
| ZL/ZR bei Classic/Pro/GamePad | L2/R2 |
| Stick-Klicks bei Wii U Pro/GamePad | L3/R3 |
| MINUS bei Classic/Pro/GamePad | SELECT |
| PLUS bei Classic/Pro/GamePad | **PS2-START während Emulation**, Fortsetzen im Launcher |
| HOME | Pause während Emulation / zurück im Launcher |
| HOME+PLUS bei Classic/Pro/GamePad | HUD umschalten; PLUS wird dabei nicht ans Spiel gesendet |
| HOME+MINUS | Emulator verlassen |
| GameCube Z | SELECT |
| GameCube L/R-Analogweg ab Schwellwert | L2/R2; digitaler Vollanschlag zusätzlich L1/R1 |
| Wii Remote A/B/1/2 | Cross/Circle/Square/Triangle |
| Nunchuk C/Z | L1/R1 |

Die bisherigen Remote-/GameCube-Kombinationen bleiben erhalten: START/PLUS pausiert, Z/MINUS+A sendet PS2-START, Z/MINUS+START/PLUS schaltet das HUD um. Z/MINUS+B führt zur Pause; Z/MINUS+B+START/PLUS beendet.

Die Stickachsen sind kalibriert, mit 12% Totzone, auf PS2 0..255 skaliert, Mitte 128 und korrekter Y-Richtung. GamePad nutzt den von libwiidrc dokumentierten Bereich ungefähr -75..75. Der GameCube-USB-Adapter verwendet den nominalen Mittelpunkt 128.

**Analogmodus:** Achsen werden laufend an den vorhandenen SIO2-/PADMAN-Backend übergeben. Die Gast-Moduswahl wird dabei nicht zwangsweise überschrieben. Die bestehenden PADMAN/XPADMAN-HLE-Befehle können den Analogmodus wählen; vollständige DualShock2-Drucksensorik, zusätzliche SIO2-Konfigurationsbefehle und Rumble sind weiterhin offen.

## Wii U und IOS

Dies bleibt eine Wii-Homebrew-DOL, kein nativer Wii-U-/Cemu-Build. Auf echter Wii U ist die Gesamtkompatibilität noch nicht bestätigt. GamePad setzt passende Wii-VC-Firmware voraus. Die Firmware wird von der DOL **nicht** installiert oder gepatcht.

Falls die libwiidrc-Firmware erkannt wird, verweigert die optionale ARM-Startauswahl jetzt den Reload auf IOS222 mit der Meldung `Wii VC input requires current IOS`. So bleibt der GamePad-Startkontext erhalten. Bei anderen Starts bleiben die bisherigen IOS-Regeln erhalten; ein initialer vWii-Test sollte ARM/IOS222 ausgeschaltet lassen.

USB-Lesezugriffe sind asynchron und einzeln ausstehend. Der EE-Loop wartet nicht auf Reports; fehlerhafte oder ältere als 500 ms gewordene Reports geben neutrale Eingaben. Ohne Adapter wird höchstens alle zwei Sekunden gesucht. USB-DMA-Puffer sind dauerhaft reserviert, und die gemeinsame USB-Infrastruktur für Spiel-/Datenträger wird beim Controller-Shutdown nicht deinitialisiert.

## Messbare Prüfung

- 34 native Regressionstests einschließlich R1363-Sound/Checkpoint, Cache/VU, IOP/Lightrec, PADMAN/XPADMAN und ISO bestanden.
- Neuer Mapping-Test: 50 Prüfungen für Belegung, Totzone, Achsen, Kanal 4, Gerätewechsel, Wii-U-Pro-Low-Word-Stick-Klicks, USB-Reports, Session-Kombinationen und Impulserhaltung.
- Neuer Gast-Test: GamePad-Eingaben über echte SIO2-Digital-/Analog-Antworten; negative Button-Polarität, Achsenreihenfolge und neutraler Zustand.
- USB-Transporttest mit simuliertem USB: Initialisierung, genau ein ausstehender Transfer, veraltete Reports, Fehler, Wiederverbinden, spätes Callback nach Shutdown/Reinitialisierung und gedrosselte Suche bestanden.
- 1.020 Vergleiche der **tatsächlich gelinkten PPC-Eingabefunktionen** mit nativer C-Referenz bestanden; Big-Endian-Strukturen berücksichtigt.
- Gelinkte PPC-R1363-Sound-Pipeline geprüft: DMA4, AutoDMA7, 768 Gast-Samples, PCM-Queue und IRQ bei stummer Ausgabe.
- DOL-/ELF-Abschnitte bytegleich, Lade-/BSS-Bereiche innerhalb MEM1; kein physischer Wii-/Wii-U-Boottest hier.

Build: devkitPPC r50 / GCC 16.1.0, libogc 3.1.0, gleiche Optionen wie R1363-B plus Controller-Sourcen. Der SDK-Layer wurde mit dem in R1351 dokumentierten SHA256 wiederhergestellt. Compiler/SDK, BIOS und Spieldaten werden nicht mitgeliefert.

## Erster Hardwaretest

1. DOL wie bisher starten, zunächst nur einen Controller verwenden.
2. Im Launcher Navigation, A/B und PLUS/START prüfen.
3. Tekken per Cold Boot starten. SELECT/Coin und PS2-START prüfen; auf Classic/Pro/GamePad wird PLUS nun direkt gesendet.
4. Analogsticks in einem Spiel prüfen, das Analogmodus anfordert. Danach abziehen und anderen Controller testen.
5. GamePad gesondert nur über passenden Wii-VC-Start testen; für den ersten Wii-U-Lauf ARM/IOS222 aus.
6. Log nach Pause sichern. `PAD_INPUT` enthält Gerätetyp, Host-Slot, Verbindung, GamePad-Verfügbarkeit, USB-Status und Achsen RX,RY,LX,LY.

USB-Status: 0 kein Adapter, 1 Initialisierung, 2 bereit, -1 USB-Host nicht verfügbar, -2 Transferfehler. Gerätebelegung und Transport sind im Code umgesetzt; tatsächliche Funkkopplung, USB-Zugriff und GamePad-Firmware müssen auf Hardware bestätigt werden.

## Quellen

- [devkitPro/libogc WPAD-Header](https://github.com/devkitPro/libogc/blob/master/gc/wiiuse/wpad.h)
- [libogc Classic/Wii-U-Pro-Decoder](https://github.com/devkitPro/libogc/blob/master/wiiuse/classic.c)
- [libogc WPAD-Kopie und Wii-U-Pro-Extra-Bits](https://github.com/devkitPro/libogc/blob/master/wiiuse/wpad.c)
- [FIX94/libwiidrc: Firmware-Voraussetzung und Analogbereich](https://github.com/FIX94/libwiidrc)
- [ToadKing GameCube-USB-Adapterprotokoll](https://github.com/ToadKing/wii-u-gc-adapter/blob/master/wii-u-gc-adapter.c)
- [libogc asynchrone USB-API](https://github.com/devkitPro/libogc/blob/master/gc/ogc/usb.h)

Vendorteil: FIX94/libwiidrc, MIT-Lizenz beigefügt. Lokal geändert: Include-Pfad und `WiiDRC_Reset` zum Verwerfen alter Firmware-Pointer vor IOS-Wechsel.
