# Alpha-Stand am 19. Oktober 2026

Ziel des Besitzers: erste öffentliche Alpha am Abend. Ein Termin ersetzt keine
Prüfung. Eine technische Testalpha muss ihren begrenzten Umfang klar ausweisen;
eine spielbare PS2-Alpha ist mit den aktuellen Belegen nicht zugesichert.
Aktuelle Arbeit: `codex/gt2000-demo-progression`, auf Draft-PR #49.

## Konkreter Fortschritt

- Die neu hochgeladenen fünf Split-ZIP-Teile ergeben ein vollständiges Image:
  DEFLATE-Ende, exakte Länge und ZIP-CRC stimmen. Alle Daten bleiben privat.
- Disc-Kennung `PAPX_902.03`, NTSC, GT2000 Japan Taikenban. Diese Demo ist kein
  Nachweis für die GT3-Vollversion. Das unveränderte Image erreicht in offizieller
  PCSX2 v2.8.2-Softwareausgabe per Fast Boot eine Introsequenz mit Fahrzeugen.
  Slow Boot bleibt zunächst bei der BIOS-Sprachauswahl; die Startbedingungen
  sind deshalb ausdrücklich verschieden.
- Zwei ursprüngliche lokale Gekko2-Kaltstarts, je 240 Millionen IOP-Slots,
  führen den Demo-ELF aus, aber produzieren kein Scanout. Keine CPU hält an;
  VU1 bleibt inaktiv, GIF bleibt bei den 88 Initialisierungs-Qwords.
  Experimentelle und präzise Schedulerkonfiguration zeigen diesen Zustand.
  Die Host-Ereignisstatistik weist dabei keine ausgeführten PPC-Eventbatches
  aus; dies ist kein vollständiger Hardwarevergleich beider Scheduler.
- Die isolierte CDVD-Schleife fragt SCMD GetError (4), DiskReady und NCMD Read
  ab. GetError antwortete fälschlich mit dem generischen Boolean 1 und schrieb
  zusätzlich einen zweiten, unbestellten Antwortwert. Jetzt gibt die RPC genau
  einen tatsächlichen Fehlercode aus dem gemeinsamen Laufwerkszustand zurück,
  ohne das MMIO-Fehlerregister zu löschen und ohne kurze Antworten zu überlaufen.
- Der korrigierte frische Demo-Kaltstart kommt in ein VBlank-Polling, weiterhin
  ohne konfigurierte Bildausgabe. Das ist begrenzter Fortschritt und kein
  Spielstart-Fix. Ursache des VBlank-Wartens ist noch nicht isoliert.
  Ein anschließender 30-Slices-Resume mit ausgeschalteten Event-/Idle-Batches
  bleibt ebenfalls dort; ein Resume ersetzt keinen unabhängigen Kaltstart.

Verhaltensreferenz: ps2sdk `scmd.c::sceCdGetError` und
`cdvdfsv.c::cdvdfsv_rpc3_04_geterror`, Commit
`329e2bb43287e16af16c4dde1c614e2662c24468` (AFL-2.0). Referenzquellen wurden
außerhalb des Checkouts gelesen, nicht in den Emulator kopiert.

## Ausgeführte Prüfungen und Prüfpaket

Runtime-Quellcommit `ccb7d5c`: 90 Hostgruppen PASS, darunter 2.912 neue
GetError-Prüfungen für echte Fehlerwerte, kurze Antworten und Sentinelbytes.
Die neue Regression schlägt gegen den vorherigen EE-Kern wie erwartet fehl.
Auf beiden aktuellen Wii-ELFs laufen je 144 tatsächliche PPC-RPC-Transaktionen
sowie die maskierten EE-Fenster und Scheduler-/Mailbox-Vergleiche erfolgreich
(sechs gelinkte Prüfgruppen insgesamt). Die 18 Gruppen aus #49 gehören zum
vorherigen Build und werden hier nicht als erneut ausgeführt ausgegeben.

devkitPPC r50/GCC16.1.0, libogc3.1.0, bisherige SPEED/AOT/DSI/GX-Fence-Flags:
DOL-/ELF-Identität und statisches MEM1 bestehen bei beiden Varianten. Rest:
2.202.828 Bytes mit Chain-Buildflag, 2.339.532 Bytes im Kontrollbuild.
Keine neue physische Wii-/Dolphin-Ausführung oder gemessene Gesamt-FPS.

`tools/package_alpha_review.py` erzeugt ein privates SD-Prüfpaket. Es prüft die
90/6-Ergebnisse, aktuelle ELF-Hashes, eingebettete Build-ID, erneute DOL-/ELF-
und MEM1-Gates sowie die Bytegleichheit des verpackten DOL. Es liest nur explizit
benannte synthetische Prüfdateien; Besitzer-BIOS, Disc, Bilder und Logs gelangen
nicht ins Paket. Der Kontrollbuild ohne einkompilierte Kettenresidenz ist gewählt.
Es wird keine öffentliche GitHub-Release angelegt und keine Alpha-Versionsnummer
behauptet; die Runtime bleibt BETA12.33 mit eindeutiger Commitkennung.

```sh
python3 tools/test_alpha_compat_host.py --output-json PRIVATE_REPORTS/host.json
python3 tools/package_alpha_review.py --reports PRIVATE_REPORTS \
  --output OUTSIDE_CHECKOUT --build-commit ccb7d5c
```

Die benötigten `linked.json`, `dol-{chain,control}.json` und
`memory-{chain,control}.json` müssen aus den beschriebenen echten Prüfungen
stammen. [Ergebnisse](verification/GT2000-DEMO-RESULTS.json) enthalten die
konkreten Checks und Build-Hashes; die Spiel- und BIOS-Daten bleiben privat.

## Was für eine technische Alpha noch nachzuweisen ist

| Punkt | Stand / Freigabekriterium |
| --- | --- |
| Exakter Kandidat | Quellcommit, DOL/ELF-Hash, Toolchain und aktive Optionen dokumentieren; kein umetikettierter Altbuild. |
| Regressionen | Neue CDVD-Prüfungen, Hostsuite, PPC-Dispatch und beide Wii-Buildgates ausführen; Ergebnisse getrennt von Hardwarebelegen führen. |
| Wii-Abnahme | Einen Kandidaten direkt auf Wii prüfen: HBC-Start, Controller, Einstellungen speichern, Cold Boot, erste BIOS-Ausgabe, Rückkehr ins Menü und HBC; kein dauerhaft hängendes GX. Agent hat keinen Wii-Zugriff. |
| Sichere Vorgaben | Event-Scheduler, 128-Bit-Kettenresidenz, experimentelle PSM-Konvertierung und GX-Risiko bleiben opt-in. Bestehende SD-Einstellungen müssen beim Test ausdrücklich berücksichtigt werden. |
| Öffentlicher Umfang | README nennt extrem niedrige Geschwindigkeit, beschädigte/fehlende BIOS-Flächen und unbestätigten Gameplayfortschritt. Keine 30-FPS- oder breite Titellisten-Zusage. |
| Sauberes SD-Paket | Nur eigene DOL/Metadaten/Icon/Anleitung; keine BIOS-, Disc-, NVM-, Karten-, Dump- oder Besitzer-Logdateien. Paket-DOL muss mit geprüftem DOL bytegleich sein. |
| Rückkehrmöglichkeit | Bisheriges funktionierendes Paket und Einstellungen sichern; neue Testdaten überschreiben keine Karten oder BIOS-Dateien. |

## Technische Hürden für echte Spieleunterstützung

1. **Startpfad und IRQs:** Demo wartet nach der korrigierten Fehlerabfrage auf
   VBlank. Keine zusätzlichen IRQs erfinden, keine Schleife durch einen PC-Hack
   überspringen. Das Ereignis muss über den gemeinsamen Hardwarepfad ankommen.
2. **CDVDFSV-Datentransfer:** NCMD READ ohne Antwortpuffer ist ein regulärer
   asynchroner Aufruf (`sceCdRead` mit Callback). Der alte MMIO-Reentry-Adapter
   lehnt `call_recvbuf == 0` ab; der gemeinsame Fallback quittiert trotzdem.
   Sektorübertragung zum EE, Alignment-Metadaten, Callback und tatsächliche
   Fehler müssen zusammen implementiert und gegen synthetische Images geprüft
   werden. Ein pauschal erfolgreicher Rückgabewert ersetzt keine Daten.
3. **Allgemeine Semantik:** IRX-Laden ist nicht gleich Modulausführung. SIF/RPC,
   EE/IOP-Wakeup-Reihenfolge, VU-Pipeline und GX-Fallbacks bleiben teilweise
   implementiert. Bestimmte Teilkorrekturen aus #49 belegen keine Vollständigkeit.
4. **Geschwindigkeit:** Das reale BETA12.32-Profil zeigt hohe GS-, EE-Dispatch-,
   Scheduler- und Native-Versuchsanteile. Die Working-Set-Ergebnisse des Wii-
   Benchmarks begründen kleine, zusammenhängende Hot-Daten und weniger Dispatch-
   Übergänge. Sie liefern keinen Multiplikator für Gesamt-FPS. Warme EE-Fenster
   sparen PPC-Instruktionen; kalte Übersetzung kann teurer sein. Den Modus erst
   nach vollständigen Workload-Vergleichen allgemein einschalten.

Die Alpha kann deshalb nicht ehrlich als Abschluss dieser vier Emulatorbereiche
oder als bestätigte Unterstützung von GT3, Crash oder Tekken angekündigt werden.
Das Veröffentlichungsziel wird als technische Vorschau vorbereitet; eine
öffentliche Freigabe wird in diesem Arbeitsstand nicht automatisch durchgeführt.
