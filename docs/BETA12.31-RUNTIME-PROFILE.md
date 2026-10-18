# BETA12.31 – Laufzeitprofil, Wii Limits 1.1 und private BIOS-Untersuchung

Basis BETA12.30 und Wii Limits 1.0, eigener Branch
`codex/runtime-profile-bios-investigation`. Kein abgeschlossener BIOS-Grafikfix
und kein gemessener FPS-Gewinn. Bisherige JIT-/GX-Optimierungen, EABI-Saves,
Invalidierungen und experimentelle Standardwerte bleiben erhalten.

## Emulatorprofil

`PROFILE_COST schema=1` im normalen Wii-Sitzungslog enthält exklusive Ticks,
Scope-Eintritte und Arbeitsmengen je Kategorie. Neue Kategorien für native
EE-Versuche und VU0/VU1; übriges EE bleibt Dispatch/Scalar/Services und ist
kein reiner Interpreterzähler. Native Versuche schließen abgelehnte Versuche
mit null Retirement sowie separat erfasste Compile-Zeit ein. Zusätzliche
Arbeitsmengen: EE-Step-Retirements, native Guest-Retirements, VU-Paare,
gepackte GX-Bytes und erfolgreich importierte Readback-Pufferbytes.
Keine pro-Instruktion-SD-Ausgabe; Daten außerhalb der Messintervalle im
bestehenden gedrosselten Sitzungslog. Arrays werden nur bei aktivem bestehendem
Sample/Present-Profil aktualisiert; Instrumentierung selbst hat Aufwand.

`entries` zählt Start/Enter, nicht fortgesetzte Restores oder erfolgreiche
Instruktionen. `work` hat kategorienbezogene Einheiten, ist nicht addierbar;
EE-Step-Retirements enthalten auch native Retirements und keine ausgelassenen
Idle-Skip-Schritte. Gepackte Bytes sind keine gemessene GPU-Busübertragung.
Sample-CPU und exakt erfasste Present-Aufrufe bleiben getrennte Nenner.
Alte TIME_SAMPLE-Zeilen bleiben vorhanden; neue Kategorien ausschließlich in
PROFILE_COST vollständig auswerten. Keine Scope-Prozentwerte als Anteil der
gesamten Emulatorlaufzeit oder als Auslastung ausgeben.

```
python3 tools/analyze_runtime_profile.py PRIVATE_SESSION_LOG --output-json /tmp/runtime-costs.json
```

Auswerter übernimmt nur strukturierte Profilfelder, keine Gastregister oder
privaten Dateipfade. Er trennt die Scope-Prozentwerte und verwirft ungültige
Einheiten, Zahlen, doppelte Felder und gemischte Timerfrequenzen. Mehrere
Sitzungen nur bewusst gemeinsam auswerten; Reproduktion mit gleicher Szene,
Cold-Boot-Optionen, Steuerung und Messdauer. Danach den größten Zeitverbrauch
mit isoliertem Benchmark vergleichen, eine Optimierung ändern und erneut messen.

## Wii Limits 1.1

Vorhandene Tests bleiben; zusätzlich bidirektionale MEM1/MEM2-Kopien in fünf
Größen, jeweils 32-Byte-ausgerichtet und um vier Bytes versetzt. Jede Kopie
außerhalb des Intervalls vollständig geprüft, Puffer besitzen 32 Reservebytes.
Gesamtlauf 280 Zeilen mit fünf Samples je Fall. Bestehende CSVs bleiben erhalten.
Keine IOS-Änderung oder BIOS-Nutzung im Benchmark. [Benutzung](../apps/wii_limits/README.md).
Wii-Messwerte einer früheren Besitzer-CSV bleiben privat und werden nicht
als neue 1.1-Messung oder Emulator-FPS veröffentlicht.

## Private BIOS-Diagnose

Besitzer-BIOS nur außerhalb des Checkouts: ROMDIR-/ROMVER-Inventar, OSDSYS-ELF,
partielle generische MIPS-Disassemblierung und unveränderter Host-Kaltstart.
Nach 240 Millionen IOP-Slots 45 echte Display-Aufnahmen, EE-/IOP-Halt nicht
beobachtet. Das umfasst Startanimation/Logo/Überblendungen; ein konkreter
Kacheldefekt wurde dadurch noch nicht unabhängig isoliert. Keine Gastcode-,
PC-, RAM- oder künstliche Bildreparatur. Vorhandene Legacy-Injektionen bleiben aus.

```
# Capstone optional, z.B. in einem eigenen venv; ohne Paket ehrlicher not_run-Status.
python3 tools/investigate_bios.py --bios PRIVATE_BIOS --output /tmp/private-bios --slices 240
```

Ausgabe innerhalb des Repositories wird abgelehnt. Der Hosttreiber setzt nur
RTC reproduzierbar, führt echte Schedulerarbeit aus und schreibt private
Bild-/RAM-/Checkpoint-Dateien. Optionaler Resume/Input-Plan nutzt echte Zustände
und Controllerereignisse. Generischer MIPS64-Decoder deckt R5900/MMI nicht
vollständig ab, Code-/Datenunterscheidung im rohen RAM ebenfalls begrenzt.
Die ELF-Disassemblierung umfasst Loadercode; für die entpackte OSDSYS-Routine
muss der private RAM-Lauf betrachtet werden. Reines Disassemblieren erklärt
keine Pixel ohne zugehörige GS-Pakete, Speichertransfers und Renderzustände.

BIOS, abgeleitete Binärdateien, Bilder, Disassemblierung, Profile, Checkpoints
und Besitzerlogs werden niemals in PRs oder Pakete übernommen. Öffentliche
Tests nutzen nur selbst erzeugte Daten. Ein historischer Hosttreiber-Linkfehler
für optionale Idle-Stats ist korrigiert; keine Ersatzsymbole erfunden.

## Prüfung und verbleibende Arbeit

78 bisherige Host-Suites bestanden nach den Core-Änderungen. Neue Tests prüfen
Zähler, echte EE/VU/GX-Aufrufe, unabhängige Analyse-Nenner und synthetische
ROMDIR-Grenzen. Wii Limits 1.1 besteht 87 Host-Kernel/Parser-Prüfungen und 135
Aufrufe des tatsächlichen ELF-PPC-Codes inklusive libc-Kopien über beide
modellierten Speicherbereiche, Guards, Ausrichtungen, EABI und Fence-Timeout.
Diese PPC-Modelle sind kein physischer Wii-Lauf. Weitere Build-/PPC-Ergebnisse
werden nach Durchführung ergänzt.

Offen: genauer JIT-Instruktionsmix/Codecache-/Callbenchmark, GX-Batch- und
Texturwechselmatrix, vollständige Readbackkosten inklusive CPU-Konvertierung,
Wii-Laufzeitprofil derselben Szene und unabhängige Referenzbilder/-GS-Traces
für die BIOS-Kacheln. Keine voreilige Speicherverlagerung oder Entfernung von
Fences allein auf Basis eines Mikrobenchmarks.
