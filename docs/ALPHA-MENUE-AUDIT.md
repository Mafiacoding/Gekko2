# Vollständiger Menüaudit für die Wii-Alpha

17.10.2026, BETA12.27 / `c51ab567`. Dieser Text entscheidet die geplante
Aufteilung; die Oberfläche selbst wird mit diesem Audit noch nicht umgebaut.
Umsetzung und Gates: [A01 im Arbeitsplan](ALPHA-ARBEITSPLAN.md#a01--menü-und-konfigurationsvertrag-p0).

## Reproduzierbarer Befund

```sh
python3 tools/audit_alpha_options.py --output-json /tmp/gekko2-options.json
```

Das Werkzeug kompiliert die echten Konfigurationsdateien für vier
Metadatenprofile. Es liest keine Nutzerkonfiguration und startet keinen
Emulator. Die BETA12.27-Defines umfassen die für Optionsfähigkeit relevanten
Flags des Buildskripts; Hardwarefähigkeit wird dadurch nicht simuliert.

| Metadatenprofil | requested default | active default | active nach All-OFF |
|---|---|---|---|
| Host | `00707540` | `00707540` | `00000000` |
| Wii ohne zusätzliche Testdefines | `00787df7` | `00787df7` | `00000000` |
| BETA12.27-Testbuild | `00787dff` | `00787dff` | **`00001080`** |
| Wii-Interpreter-Metadaten | `007075c0` | `007075c0` | `00000000` |

`optimization.c:gekko2_opt_apply` erzwingt bei `GEKKO2_GX_NATIVE_BIOS_TRI`
GX_RESIDENT (Bit 7) und GX_GOURAUD (Bit 12). Sie können im aktuellen Testbuild
im Menü OFF stehen, aber für die Sitzung ON sein. Das globale Renderer-Gate
`g_gx_present` bleibt separat: daraus folgt nicht, dass globales GX OFF ignoriert
wird. Vor Alpha brauchen alle drei Ebenen eine verständliche Darstellung.

`gekko2_opt_available()` prüft nur einen Teil der Buildfähigkeiten. Es meldet
zum Beispiel ARM- und Scheduleroptionen unabhängig davon, ob zur Laufzeit
ein Worker-Dienst bzw. dessen benötigte Vorgängeroptionen aktiv sind.
Runtime-Gates/Fallbacks existieren; die Menüanzeige muss sie erklären.

Alle 32 Kernoptionen haben Referenzen außerhalb der Menüsteuerung.
**Kein Schalter ist allein durch fehlende Nutzung als tot bewiesen.**
„Aus dem Nutzermenü entfernen“ bedeutet daher zuerst verbergen/bündeln,
nicht den funktionierenden Backendpfad oder die alte Config-ID löschen.

## Geplante sichtbare Struktur

- **Wii-Basisprofil:** reproduzierbare bisherige Defaults als Ausgangspunkt;
  erst nach Hardware-A/B als empfohlen markieren. Keine Beta-Schalter pauschal ON.
- **Renderer:** GX Hybrid / Software-Referenz. Beide ehrlich beschreiben;
  GX Hybrid darf präzise CPU-Fallbacks verwenden.
- **Tonausgabe:** ON/OFF; Gast-SPU2 bleibt aktiv.
- **Anzeige/Bedienung:** HUD, Gast-VBlank-/OUTPUT-Anzeige, Updatebudget;
  Ausgabe-Fit später explizit, volle Ausgabegröße bleibt Rückfall.
- **Diagnose:** Normal / Detail mit begrenzten Logs, CPU-Modi einzeln für
  Interpreter/JIT-Vergleiche; aktuelle Optionen und Fehlergründe sichtbar.
- **Experimente:** Chain-Residenz, PSM-Konvertierung und GX-Risk als drei
  eigene auswählbare Zeilen, unabhängig und standardmäßig OFF.
- **Entwickleroptionen:** Einzelbits/Custom-Profil für Reproduktion und
  Regressionen, ARM-/IOS-Seite nur mit expliziter Verfügbarkeitsanzeige.
- **Standardwerte wiederherstellen:** eigene Aktion; aktive Sitzung bleibt
  bis Cold Boot unverändert. ARM/IOS erfordern App-Neustart.

Ziel ist ein kurzer Standarddialog. Entwickler können weiterhin einzelne
Optimierungen isolieren. Ein Profil ist eine versionierte, protokollierte
Optionsmenge und keine unsichtbare Reihe weiterer Spiel-Hacks.

## Jede der 32 Kernoptionen

ID und Default beziehen sich auf den BETA12.27-Testbuild. ON sagt nichts
über Hardwarebestätigung aus. Präfix aller Schlüssel: `GEKKO2_OPT_`.
Quellreferenzen je Option liefert das Audit-JSON automatisch.

| ID / Schlüssel | Aktueller Menüname / Default | Entscheidung | Begründung / Abhängigkeit |
|---|---|---|---|
| 0 `EE_JIT` | EE dynarec / ON | CPU-Profil; Diagnose einzeln | Fallback behalten; OFF ist Referenzmodus, kein nutzloser Schalter |
| 1 `IOP_JIT` | IOP dynarec / ON | CPU-Profil; Diagnose einzeln | Teil des produktiven IOP-Pfads; Lightrec-Pilot ist kein zweiter vollständiger IOP-Core |
| 2 `VU_JIT` | VU0 / VU1 dynarec / ON | CPU-Profil; Diagnose einzeln | timed Native-/AOT-Pfade; Interpreter für Konformität erhalten |
| 3 `EE_BLOCKS` | EE native blocks / ON | mit EE-JIT bündeln | Ohne EE_JIT wirkungslos; konkrete Build-/Schedulerfähigkeit prüfen |
| 4 `NATIVE_LINKS` | Native block links / ON | internes CPU-Profil | Bounded Ketten; benötigt EE-JIT/Blocks, kein universeller Linker |
| 5 `RESIDENCY` | GPR residency / ON | internes CPU-Profil | Lokale Wortresidenz ist nicht der separate 128-Bit-Chain-Schalter |
| 6 `CODE_ARENA` | 6 MiB code arena / ON | feste Basis, Entwickleroverride | Codeowner/DSI/Lightrec hängen daran; OFF spart das statische Array nicht |
| 7 `GX_RESIDENT` | GX resident depth / blend / ON | Rendererprofil | Im Testbuild derzeit erzwungen; globales GX, Zielzustand und Fallback relevant |
| 8 `GX_SOURCE_CACHE` | GX texture reuse / ON | Rendererprofil | Reale Decode-/Hashreuse mit Invalidation; isolierbarer Diagnoseschalter |
| 9 `FASTMEM` | Fastmem RAM page cache / OFF | separates CPU-Experiment | Name mischt RAM-Cache und DSI-LW-Pilot; Arena, MMU/DSI und präziser Fault-Fallback nötig |
| 10 `GX_DISJOINT_WRITES` | GX independent VRAM writes / ON | Rendererprofil | Vermeidet Readback bei disjunkten CPU-Writes; Alias-Beweis zwingend |
| 11 `WORD_ALLOCATION` | Native load / store reduction / ON | internes CPU-Profil | Emitteroptimierung; normale Nutzer brauchen keinen eigenen Schalter |
| 12 `GX_GOURAUD` | GX Gouraud shading / ON | Rendererprofil | Name ist nach gemeinsamen Shadern zu eng; im Testbuild ebenfalls erzwungen |
| 13 `CACHE_REUSE` | Block cache reuse / ON | internes CPU-Profil | Vier Ways und begrenzte Emission; Messung statt pauschaler Cachegröße |
| 14 `HLE_RAM` | HLE RAM bulk operations / ON | CPU-Profil | Bewiesene RAM-Helper; MMIO-/Modul-/Bounds-Verträge behalten |
| 15 `STRICT_HLE` | Strict BIOS HLE fallback / OFF | Diagnose/Kompatibilität | Unbekannte Aufrufe im Gast ausführen; nicht als schneller/sicherer Universalmodus versprechen |
| 16 `ARM_WORKER` | ARM IPU worker / OFF | aus Standardmenü; ARM-Entwicklerseite | IOS-Dienst und Handshake erforderlich; nur CSC, keine allgemeine zweite CPU |
| 17 `COMPACT_CACHE` | EE compact cache (1024 owners) / OFF | Cache-Policy in Entwicklerseite | Nur mit CACHE_REUSE sinnvoll; 1024/16384 Owner nach gemessenen Miss-/Compilekosten |
| 18 `ARM_IOS222` | ARM startup IOS / CURRENT | aus Standardmenü; ARM-Entwicklerseite | Auswahl nur für tatsächlich vorhandenes unterstütztes IOS und Worker; nie automatisch reloaden/installieren |
| 19 `EE_PREFIX` | EE cached block prefixes / ON | internes CPU-Profil | Benötigt EE-JIT/Blocks; Wiederverwendung kurzer Grants erhalten |
| 20 `EE_SERVICES` | CPU inactive service guards / ON | internes CPU-Profil, Beschreibung aufteilen | Bündelt Guards, Grantgröße und VU-Reentry; kein unabhängiger „mehr Speed“-Knopf |
| 21 `IOP_DEADLINES` | IOP alarm / delay deadlines / ON | internes CPU-Profil | THREADMAN-Scans bis Deadline vermeiden; Reihenfolge unverändert prüfen |
| 22 `LIGHT_DIAGNOSTICS` | Light runtime diagnostics / ON | Diagnoselevel | Kein Performancehack; SESSION/Fault-/Timinglogs weiterhin erhalten |
| 23 `ARM_CPU_RAM` | ARM BIOS RAM jobs / OFF | aus Standardmenü; ARM-Entwicklerseite | Große RAM-Jobs mit CPU-Rückfall/Verifikation; IPCkosten können Gewinn aufzehren |
| 24 `NATIVE_EVENTS` | EE native event windows / OFF | CPU-Experimentprofil | Benötigt EE-JIT/Blocks und EE_SERVICES für Blockroute; genaue Eventgrenzen |
| 25 `EVENT_SCHEDULER` | EE / IOP event batches / OFF | Scheduler-Experimentprofil | Systemquanta+EE-JIT/Blocks nötig; gemeinsam sichtbares MMIO synchronisiert |
| 26 `IOP_IDLE_BATCH` | IOP verified idle batches / OFF | Scheduler-Experimentprofil | Nur bewiesenes Parken vor nächsten Peripherie-/Threaddeadlines |
| 27 `LONG_EE_BLOCKS` | EE long blocks (32 instructions) / OFF | CPU-Experimentprofil | Mehr als acht nur bei ausreichendem Grant; Codesize-/Compilekosten beachten |
| 28 `IOP_SERVICE_HLE` | IOP registered service HLE / OFF | Diagnose/Kompatibilitätsexperiment | Nur registrierte unveränderte Dienste; keine generischen Erfolgsantworten |
| 29 `IOP_CATCHUP_BLOCKS` | IOP catch-up native blocks / OFF | Scheduler-Experimentprofil | Nutzbar bei geschuldeten IOP-Ticks, IOP_JIT und Eventroute; Einzelmessung vor Bündelung |
| 30 `IOP_BLOCK_GROWTH` | IOP adaptive block cache / OFF | internes CPU-Experimentprofil | Benötigt IOP-JIT/Blockreuse; Wachstum nur bei größerem Grant und passendem Code |
| 31 `SOUND` | SPU2 sound output / OFF | normale Tonausgabe | OFF spart nicht den Gastmixer; `iop_spu2_sync` rendert weiter und unterdrückt nur Submit |

Die Bündelung erfolgt erst nach Dependency- und A/B-Prüfung. Bestehende
experimentelle OFF-Werte werden nicht durch eine Menübereinigung aktiviert.

## Drei separate Experimente

| Option | Heutiger Zugriff / Datei | Entscheidung |
|---|---|---|
| 128-BIT JIT CHAIN RESIDENCY – EXPERIMENTAL | Y; `sd:/pcsx2/jit-chain.cfg` | Behalten, normale auswählbare Zeile; OFF; Buildflag + EE-JIT/Blocks/Links/Residency prüfen |
| EXPERIMENTAL PSM CONVERSION | X; `sd:/pcsx2/gs-compat.cfg` | Behalten hinter Experimente; OFF; keine stillschweigende Farbtiefenumrechnung |
| GX RISK MODE – FAST TRIANGLES | virtuelle Zeile 32; `sd:/pcsx2/gx-risk.cfg` | Behalten hinter Experimente; OFF; RGB-Kantenrisiko beschreiben, keine Speicherschutzabschaltung |

Sie dürfen nicht zu einem einzigen „Turbo/Risk“-Schalter verschmelzen: Sie
verändern unabhängige Pfade und müssen einzeln vergleichbar bleiben.
Cold Boot übernimmt alle drei; START/Resume nicht. Bei fehlenden Fähigkeiten
den konkreten Grund anzeigen, statt ein wirkungsloses ON anzubieten.

## Vier Frontend-Einstellungen

| Einstellung / bisheriger Default | Entscheidung | Konkreter Befund |
|---|---|---|
| HUD / OFF | mit Diagnose-/Anzeigestufe bündeln | Hilft bei Fehlern; kann Darstellung/Presentkosten verändern |
| UPDATE: THROUGHPUT / ON | als Eingabereaktion 50/20 ms erklären; Entwicklerwert bei Bedarf | Setzt Ziel-Walltime des adaptiven Hostbudgets; ändert nicht die PS2-Taktrate |
| FPS COUNTER / ON | behalten, Metriken eindeutig benennen | Gast-VBlankrate und OUTPUT sind keine garantierten einzigartigen Spielframes |
| GX (EXPERIMENTAL) / OFF | explizite Rendererwahl | Cold-Boot-Wunsch nur im Prozess; aktuell wie HUD/Update/FPS nicht als Frontendkonfig auf SD gespeichert |

Eingriffspunkte: `source/main.c` globale Defaults/Settings-Input und
`frontend_next_budget`. Alle vier bei der Konfigmigration speichern;
Fehler nicht mit einer erfolgreichen Speicherung melden.

## Sieben Hauptaktionen

BIOS/OSDSYS, Start Disc, Select Disc, Settings, About und Exit to HBC behalten.
Cold Boot / New Log behalten, eindeutig von Resume trennen; Resume ist derzeit
START/PLUS und darf als sichtbare Aktion angeboten werden. Kein Menüpunkt ist
überflüssig, nur weil er nicht beschleunigt. About nennt die tatsächliche Build-
und Optionskennung; normale Nutzer müssen keine JIT-/IOS-Details lesen.

## Abnahme des Umbaus

1. Alle 32 IDs unverändert migrieren; alte Configs v1..v9 und drei Experimentdateien
   lesen. Neue Struktur mit Versionierung, transaktionalem Save und Backup.
2. Abhängigkeitsmatrix: EE-JIT OFF, Blocks OFF, GX OFF, nicht kompilierter
   Chain-/Riskpfad, fehlender ARM-Dienst, Arena OFF und vorhandene alte Werte.
3. requested/active nach Umschaltung, Resume, Cold Boot und App-Neustart prüfen;
   kein laufendes Nativecache mit neuer Konfiguration betreiben.
4. Profile im Log vollständig expandieren; Customwerte nicht unbemerkt verlieren.
   Werkseinstellungen beeinflussen nur gewählte neue Konfiguration, keine Karten.
5. Wiimote, GC und Nunchuk: alle Optionen ohne X/Y oder Schulterknöpfe erreichbar;
   keine Überlappungen bei 640×480, Scrollanzeige und aktuelle Auswahl sichtbar.
