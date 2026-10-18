# Gekko2 Wii Limits 1.0

Eigenständige Homebrew-App für die originale Wii. Sie liefert messbare
Vergleichsgrößen für Optimierungen, keinen universellen Grenzwert und keine
automatische Optimierung aller Spiele. Bestehende Gekko2-Dateien unverändert.

1. SD-ZIP auf eine FAT-SD entpacken: `apps/gekko2-wii-limits/boot.dol`,
   `meta.xml`, `icon.png` müssen zusammen liegen.
2. Im Homebrew Channel **Gekko2 Wii Limits** starten. **A** beginnt.
3. Bis „Finished“ warten. **HOME** oder GameCube **START** beendet;
   während des Tests bricht es zwischen Arbeitsschritten ab. RESET bricht ab.
4. Die neue `sd:/gekko2-bench/wii-limits-XXXX.csv` privat zurückgeben.
   Vorhandene Messdateien werden nicht überschrieben. SD-Fehler werden angezeigt.

Keine BIOS-/Spiel-/Kartendateien nötig. Keine IOS-Installation, Taktänderung,
Übertaktung oder Datenträgerformatierung. Kein Starlet-Worker erforderlich.
Dolphin darf zum Funktionstest dienen; dessen Zeiten sind keine Wii-Messwerte.

Fünf Messungen pro Fall. CPU- und Kopier-/Ladetests kalibrieren die Arbeitsmenge
in endlichen Schritten auf ungefähr 40 ms, mit festen Obergrenzen. GX arbeitet
mit festen kleinen Batches; GPU-Staus brechen die weiteren GX-Tests ab. Nicht
als Dauer-Stresstest oder zuverlässige GPU-Hang-Recovery ausgelegt.

Getestet werden abhängige Integer-/Float-Rechenketten, MEM1/MEM2 memcpy,
abhängige 32-Byte-Ladezyklen bei 8/32/256 KiB und 2 MiB Arbeitsmenge,
D-Cache-Flush, GX-Kleingeometrie/Füllung/Blend/Alpha-Verwerfen, RGBA8-Textur
nearest/linear, CPU/GPU-Überlappung, RGBA8-EFB-Kopie und CPU-EFB-Peeks.

Auswertung aus dem Repository:
```
python3 tools/analyze_wii_limits.py /path/to/wii-limits-0001.csv --output-json /tmp/analysis.json
```
Für Emulatorentscheidungen zusätzlich dieselbe Spielszene mit Gekko2s
Laufzeitprofil messen. Aus dem isolierten Benchmark allein folgen keine sicheren
JIT-/GS-Fixes, keine höhere FPS und keine vollständige PS2-Unterstützung.
