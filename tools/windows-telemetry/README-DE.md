# Gekko2 Netzwerk-Leistungslogger (Windows)

Einmal einrichten, dann pro Test nur eine ZIP-Datei. Keine direkte Verbindung
mit einer Chat-Sitzung, kein Fernzugriff auf die Wii und keine automatischen
Codeaenderungen. Es werden feste Leistungszaehler uebertragen, keine BIOS-,
Spiel-, Karten-, Register-, Pfad- oder Speicherinhalte. UDP ist unverschluesselt
und fuer das eigene lokale Netzwerk gedacht; keine Router-Portfreigabe noetig.

## Einmalige Einrichtung

1. Python 3.9 oder neuer fuer Windows installieren, falls noch nicht vorhanden.
   Die CMD-Dateien verwenden `py -3`, alternativ `python`. Keine Zusatzpakete.
2. Diesen Ordner auf den Windows-Rechner entpacken; die Wii-SD einstecken.
3. `SETUP-SD.cmd` doppelklicken. Die LAN-IPv4 des Windows-Rechners und den
   SD-Pfad, z.B. `E:\`, angeben. Moegliche Rechner-Adressen werden angezeigt.
   Bei mehreren Adaptern die mit der Wii gemeinsam genutzte LAN-Adresse nehmen.
4. Das Programm schreibt ausschliesslich `pcsx2/telemetry.cfg` auf die SD:
   `GEKKO2_TELEMETRY 1 192.168.1.20 26890` (Adresse ist nur ein Beispiel).
   CPU-, Renderer- und Experiment-Einstellungen bleiben erhalten.
5. Die neue BETA12.31b-App auf die SD kopieren, SD zurueck in die Wii.

## Ein Test

1. `START-LOGGER.cmd` auf dem Rechner doppelklicken. Falls Windows nachfragt,
   Python nur im privaten Netzwerk zulassen (UDP-Port 26890).
2. Gekko2 vollstaendig aus dem Homebrew Channel neu starten und die gewuenschte
   Emulation starten. Rechner und Wii muessen einander im Netzwerk erreichen.
3. Auf dem Rechner erscheinen Pakete/Optionen/Timeouts. Der Empfang endet
   automatisch zehn Minuten nach dem ersten gueltigen Paket. `Strg+C` endet
   frueher und erstellt ebenfalls den Bericht. Ohne Daten nach zwei Minuten
   wird ein leerer Bericht mit Hinweis erzeugt.
4. Nur `reports/gekko2-network-....zip` zur Auswertung bereitstellen. Die ZIP
   enthaelt normalisierte Messpakete und die automatische Zusammenfassung.
   Die Daten werden nicht selbst ins Internet oder an Dritte hochgeladen.

Fehlende Pakete werden gezaehlt; UDP garantiert keine Zustellung. Der Sender
wartet nicht auf einen Empfaenger, und die volle Vier-Paket-Warteschlange
verwirft neue Pakete. `NETWORK_TELEMETRY state=2` im SD-Log bedeutet nur, dass
der Socket bereit ist; erst der Empfang am Rechner bestaetigt den Datenweg.
`state=3` meldet fehlgeschlagene Netzwerkinitialisierung. Die SD-Logs bleiben
unveraendert als Rueckfall; bei einem harten Absturz kann der letzte Ausschnitt
fehlen. Detaillierte GX-Befehlsdiagnosen stehen weiterhin nur im SD-Fehlerlog.

Deaktivieren: `pcsx2/telemetry.cfg` loeschen oder Inhalt
`GEKKO2_TELEMETRY 0` setzen und Gekko2 aus HBC neu starten. Fehlt die Datei,
wird kein Netzwerkthread gestartet. Bei geaenderter Rechner-IP SETUP wiederholen.

## Automatische Auswertung und Grenzen

Die Zusammenfassung trennt Build, Boot, Sitzung, Profil-Epoche, aktive CPU-Maske,
Renderer und Experimente. CPU-Stichproben und exakt gemessene Praesentation
haben getrennte Prozentgrundlagen. Kumulative Profilwerte verhindern doppeltes
Addieren und ueberbruecken Zwischenverluste; fehlende letzte Pakete bleiben eine
Luecke. Intervalldaten sind bei Paketverlust unvollstaendig. Interpreter-Konfig,
GX-Timeouts und Uebertragungsverluste erzeugen Hinweise. Native Versuchszahlen
beweisen keine nativen Retirements; Zaehler mit verschiedenen Einheiten nicht
addieren. Unterschiedliche Spielszenen sind kein gueltiger Geschwindigkeits-
vergleich. Maximal 10.000 Pakete pro Empfang, keine Endlosaufzeichnung.

Dies ist kein neuer JIT-/GX-Fix, kein Test aller Spiele und kein Beleg hoeherer
Wii-FPS. Netzwerk-CPU-Kosten sind nicht null. Der isolierte Wii-Limits-Benchmark
bleibt erhalten; der Logger ergaenzt ihn um Messwerte aus dem echten Emulator.
