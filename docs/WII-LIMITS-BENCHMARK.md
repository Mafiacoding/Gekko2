# Eigenständige Wii-Benchmark-DOL 1.0

Basis BETA12.30 `baee608`, eigener Branch `codex/wii-limits-benchmark`.
Keine Emulatorsemantik, Optionen oder bisherige DOL geändert.
[Benutzung](../apps/wii_limits/README.md).

Zweck: reproduzierbare Kosten einzelner CPU-/Speicher-/GX-Arbeitsmengen messen
und mit echten Gekko2-Hotspots vergleichen. Weder maximale Hardwaregrenzen noch
Emulator- oder Spielgeschwindigkeit werden aus Mikrotests allein bestimmt.

## Messprotokoll

- Eigenständiger libogc-Prozess, normale IOS-/Interrupt-/Cacheumgebung.
  Kein IOS-Reload, Overclocking oder Starlet-Gerät. Die Laufumgebung wird als
  `wii_target_hardware_unconfirmed` protokolliert: Dolphin lässt sich nicht
  allein aus einer hochgeladenen CSV als echte Wii bestätigen.
- CSV mit Build-/Quellcommit, Compiler, Timerfrequenz, IOS, Videomodus,
  Arenagrößen, Wiederholungen, Pool, Arbeitsmenge, Einheit, Roh-Ticks,
  Submit-/Restwartezeit, Status und Ergebnis-Smoketest. Keine Seriennummer,
  Netzwerkadresse, privaten ROM-/BIOS-/Spiel-/Karteninhalte.
- Fünf Wiederholungen je Fall, CSV-Ausgabe/Flush und Ergebnisprüfung außerhalb
  der Zeitmessung. Interrupts bleiben aktiv, Ausreißer deshalb möglich.
  Auswertung zeigt Median und Spanne, keine hypothetische Auslastung.
- Kopien zählen **Nutzbytes**, nicht Lesen+Schreiben doppelt oder angebliche
  Bus-Bytes. Nearest/Linear verwenden eine weiße 256x256 RGBA8-Textur:
  dies isoliert Arbeit, aber beweist keine komplexen Texturfilter oder TEV.
- Speicher-Pools werden geprüft. MEM1: zwei freie 2-MiB-Puffer; MEM2: 4 MiB
  aus der verbleibenden Arena reserviert. Allokationsfehler sind eigene Zeilen.
  Größen 8/32/256 KiB und 2 MiB; warme Kopien und vollständiger abhängig
  geladener LCG-Ring mit einem 32-Bit-Wert je 32-Byte-Linie. Das ist eine
  Arbeitsmengenkurve, kein garantierter isolierter L1-/L2-/DRAM-Latenzwert.
- D-Cache-Flush erfasst Kosten eines Bereichs; keine behauptete Bandbreite oder
  Inhaltsverifikation. Folgerunden verschmutzen jede Linie vor dem Intervall.
- GX rendert im EFB 256x256, ohne VSync oder EFB→XFB-Present im Intervall.
  Konsole bleibt unabhängig im XFB. 16 Quads je Batch, Geometrie kleiner als
  4 KiB bei 256-KiB-FIFO. Ausgabe-Pixel werden nach Abschluss gelesen;
  Blend-Smoketest hat ±3 Toleranz und ist kein bitexakter GX-Blendnachweis.
  Alpha-Verwerfen muss den vorher schwarzen Hintergrund erhalten.
- Fence: DrawDone plus nachfolgender neuer PE-Token, Wrap eingeschlossen.
  Kein Callback-/CP-Idle als Erfolgsbeweis. Poll-Grenze 500 ms, danach kein
  weiterer GX-Workload oder Freigabe eventuell noch benutzter Puffer. Das ist
  kein allgemeiner Watchdog für libogc-/IOS-/Hardwareausfälle.
- CPU-Überlappung nutzt denselben 262144-Iterationskern wie der separate
  Referenztest. Gesamtzeit enthält Arbeit und GP-Abschluss; Restwartezeit
  beginnt erst nach CPU-Arbeit. Aus einer Differenz folgt keine CPU-Auslastung.
- RGBA8-Rückkopie mit Cache-Flush vor GP-Schreiben und CPU-Invalidate nach
  Fence. CPU-Peeks gesondert. Nominale übergebene Pixel sind keine Messung der
  nach Alpha-/Z-Test tatsächlich geschriebenen Pixel.
- Vollständiger Lauf: 180 Datenzeilen; Abbruch, fehlende Pools oder Timeout
  können weniger liefern. Fehlender Abschluss wird vom Auswerter markiert.

## Reproduktion und Prüfungen

Offizielle vorhandene Toolchain: devkitPPC r50/GCC16.1.0, libogc3.1.0,
SDK-Imagepin wie BETA12.30:
`devkitpro/devkitppc@sha256:4c919aa26151dd43d88ca28c922d1fe2409579a8ba60ef56517baf1abdfb1a48`.
Standalone bewusst O2, `-ffp-contract=off`, `-fno-builtin-memcpy`,
`-mcpu=750 -mrvl -meabi -mhard-float`, `-Wall -Wextra -Werror`.
Diese Zahlen dürfen nicht als O1-Emulatorvergleich ausgegeben werden.

```
python3 tests/test_wii_limits.py
LIMITS_OUTPUT=/tmp/wii-limits bash tools/build_wii_limits.sh
python3 tools/verify_wii_limits_linked.py /tmp/wii-limits/Wii-Limits.elf --nm "$DEVKITPPC/bin/powerpc-eabi-nm"
```

87 unabhängige Host-Prüfungen: Integer-/Float-Referenz, vollständige Ringe,
absichtliche Speicherbeschädigung, Einheiten/Median, unvollständige CSV,
Duplikate, ungültige Timerwerte und Komponenten über Gesamtzeit.
75 tatsächliche ELF-PPC-Aufrufe prüfen Kernel, Speicher und EABI sowie
verzögerten Token, Token-Wrap und permanenten GP-Stau mit **Plattformstubs**.
DOL-/ELF-Sektionsabgleich und statisches MEM1-Gate bestanden.
Keine physische Wii-/Dolphin-Ausführung und keine gemessenen neuen Wii-Raten.

## Grenzen und nächste Entscheidungen

Noch kein kompletter Instruction-Mix-/Paired-Single-/JIT-I-Cache-Test, keine
hardwareseitigen CPU-/GP-Performancecounter, keine Temperatur-/Taktmessung,
keine Messung von SPU2, DVD/USB/Netzwerk oder eines gleichzeitigen Spiels.
Kein Beleg für alle Auflösungen/TEV-/Blend-/Depth-/Feedbackzustände.
Emulator-Hotspots bleiben mit `runtime_profile` und denselben Szenen zu prüfen.
Die bestehende hochgeladene Legacy-CSV wird privat ausgewertet. Ihr Schema
enthält keine Herkunft/Schrittweite/GP-Arbeitsmenge; daraus werden keine
Peakwerte, Starlet-Fehler oder sichere GX-Optimierungen abgeleitet.
