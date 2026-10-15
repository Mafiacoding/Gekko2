# GEKKO2 R1359-B — Lightrec-Aufrufpfad

## Änderung
Der begrenzte IOP-Lightrec-Pilot aus R1358 wird jetzt auch aus der nativen Block-Vorbereitung aufgerufen. Nach erfolgreicher Ausführung wird die originale Instruktion einmal retired; Status 2 verhindert die Ausführung des alten PPC-Bodys und eine doppelte Tick-Zählung. Stale-Source- und Scalar-Recovery-Pfade können den Pilot ebenfalls verwenden. Load-Delay, Branch-Delay, IRQ/HLE und Speicherzugriffe bleiben beim vorhandenen IOP-Core.

Der 512-KiB-Codepuffer wird beim ersten gültigen nativen IOP-Grant reserviert, bevor der gemeinsame 6-MiB-MEM1-Puffer stark fragmentiert ist. Initialisierung erzeugt keine fingierten Gast-Ausführungen. Bei fehlendem Speicher, deaktivierter CodeArena oder Backend-Fehler bleibt der alte Ausführungspfad verfügbar.

LIGHTREC_ROUTE protokolliert eligible, fallbacks und native_block_hook. LIGHTREC_IOP behält ready, bodies, program_misses, failures und code_pool. ready=1 bedeutet initialisiert; erst bodies>0 bedeutet tatsächliche Ausführung. eligible=0 bedeutet, dass keine passende MULT/MULTU/DIV/DIVU-Instruktion den Aufrufpfad erreicht hat. Dieser Build ist weiterhin ein begrenzter Pilot, kein vollständiger Lightrec-IOP- oder EE-Core. Einzelinstruktionsübergaben können langsamer als vorhandener PPC-Code sein.

## EE-Befund aus Logs 34/35
Log 34 ist R1358-B und endet bei 476.897.078 EE-Instruktionen; Log 35 ist R1358-A und endet bei 355.321.041. Diese Läufe messen keinen Übergang bei einer Milliarde. Eine feste Milliardengrenze oder ein Memory Leak ist damit nicht nachgewiesen.

B hat zuletzt 348.160 freie Arena-Bytes, aber nur 9.952 Byte größten freien Nutzbereich. 104.774.720 Code-Bytes wurden kumulativ durch Druck-Verdrängung freigegeben. Das zeigt Code-Umsatz und Fragmentierung, keinen Wechsel des Codepuffers nach MEM2. Allokationsfehler waren 0.

B zeigt 6.753.889 Budget-Misses, aber auch 5.391.058 Prefix-Ausführungen mit 32.670.626 retired Instruktionen. Budget-Misses sind deshalb kein direkter Interpreter-Fallback-Zähler. Kleine Ausführungsfenster und Wiederübersetzungen bleiben plausible Bremsen; ihr jeweiliger Zeitanteil ist damit nicht bestimmt. EE-Prefix, Scheduler, Cache-Verdrängung, GX, DSI und ARM werden in R1359 nicht verändert. DSI ist in beiden Logs aktiv: A 80/80 und B 75/75 faults/patched. ARM hatte 0 abgeschlossene Aufträge.

## Prüfung
19 native Regressionen bestehen, einschließlich originaler IOP-Pipeline-Oracles, Load-Delay mit MULT, Branch-Delay und geänderter Instruktionsquelle. Neue direkte Block-Prepare-Tests prüfen genau einmalige Retirement/Ticks, stale recovery und Branch-Delay-Ziel.
16.384 echte native Host-Lightrec-Ausführungen bestehen; zusätzliche Prüfung initialisiert den Backend-Puffer ohne Ausführungszähler zu erhöhen. Das ist kein PowerPC-Lauf.
Modernes devkitPPC r50 / GCC 16.1.0 und libogc 3.1.0: DOL/ELF gebaut. DOL-Sektionen und Einstieg werden gegen ELF geprüft; kein physischer Wii-Boot bestätigt. Die zusätzliche PPC-Unicorn-ABI-Prüfung konnte wegen fehlendem Python-Modul unicorn nicht gestartet werden und gilt nicht als bestanden.

## Wii-Test
R1359-B.dol als boot.dol einsetzen. Gleiche Einstellungen wie in Log 34/35: insbesondere IOP JIT und CodeArena an. Einen Cold Boot ausführen. Die beiden neuen Lightrec-Zeilen vor und nach BIOS-Bild prüfen. Bei ready=1 und bodies=0 ist der Puffer vorhanden, aber noch keine Pilot-Instruktion ausgeführt. Bei eligible>0 und fallbacks>0 liegt ein Backend-/Initialisierungsproblem vor. Bitte das Log möglichst auch über die bisher beobachtete Einbruchphase hinaus aufnehmen; EE-Zähler und reale Laufzeit sind für den Vergleich entscheidend.

Ein Wii-Speedup ist noch nicht bestätigt. Quellcode und Drittanbieter-Lizenzen sind im Archiv enthalten; BIOS, Spiel und SDK sind ausgeschlossen.
