# Gekko2 Netzwerklogger 1.1 fuer Windows / BETA12.31c

PC-IP, Port und ON/OFF lassen sich jetzt direkt im Wii-Menue speichern.
Ein Testpaket funktioniert ohne BIOS oder Spiel und wird vom PC bestaetigt.
Python 3.9+ wird auf Windows benoetigt, keine Zusatzpakete und keine direkte
Verbindung zu einer Chat-Sitzung. Daten bleiben lokal bis du den Bericht teilst.

## Einrichtung und Verbindungstest

1. `apps/Gekko2` aus der SD-ZIP auf die SD kopieren; den Ordner `Windows-Logger`
   auf dem Windows-Rechner behalten. Gekko2 aus HBC neu starten.
2. Auf Windows `START-LOGGER.cmd` doppelklicken. Das Fenster nennt den gebundenen
   UDP-Port (Standard 26890) und moegliche PC-IPv4-Adressen. Die LAN-Adresse des
   mit der Wii gemeinsam genutzten Netzes waehlen, nicht VPN/VirtualBox/Loopback.
3. Auf der Wii: `SETTINGS > Network logger`. `Enable logger = ON` setzen;
   die vier IPv4-Oktette der **PC-Adresse** und den gleichen UDP-Port einstellen.
   UP/DOWN waehlt die Zeile, LEFT/RIGHT +/-1 (Halten wiederholt), Wiimote 1/2
   bzw. GC X/Y +/-10; bei Port +/-1000. Standardmaessig bleibt alles OFF.
4. `SAVE / APPLY` mit A waehlen. Das speichert auf SD und startet/aktualisiert
   den Sender ohne App-Neustart. `SEND TEST PACKET` mit A waehlen.
5. Erfolg: Windows meldet `TESTPAKET empfangen`; Wii zeigt
   `PC RECEIPT CONFIRMED`. Diese Anzeige belegt eine bestaetigte Zustellung,
   keine dauerhaft bestehende Verbindung. Jeder neue Test loescht die alte
   Bestaetigung. Bei jeder Zielaenderung wird ebenfalls neu geprueft.
6. Erst danach BIOS/Spiel starten. Nach zehn Minuten ab dem ersten Paket oder
   Strg+C erzeugt Windows **eine ZIP** in `reports`. Nur diese Datei teilen.
   Ein erfolgreicher Menue-Test allein ist noch keine CPU-Leistungsmessung.

## Wenn kein Paket ankommt

Nicht weitere beliebige Ports oeffnen. Fuer diesen Logger ist nur der angezeigte
UDP-Port im privaten Windows-Netzwerk erforderlich; keine Router-Portfreigabe.

Die Wii zeigt getrennt:
- `NO CONFIG - OFF` / `INVALID CONFIG - OFF`: konfigurieren und SAVE waehlen.
- `NETWORK STARTING`: DHCP/Netzwerkaufbau laeuft im Hintergrund.
- `NETWORK FAILED`: Phase (`net_init`, DHCP, Socket, nonblock, thread) und
  SDK-Fehlercode werden direkt darunter angezeigt. SAVE wiederholt den Versuch.
- `SOCKET READY - NO PC ACK`: Socket vorhanden, aber kein bestaetigter Empfang.
  Ziel-PC-IP, Port, gestarteten Logger, Windows-Netzprofil und WLAN-Isolation
  pruefen. Der Wii-IP-Eintrag muss nach DHCP eine gueltige Adresse anzeigen.
- `Sent`, `dropped`, `errors`: SDK-Sendeergebnisse, Queue-Verluste und Sendefehler.
  Ein erfolgreicher UDP-Sendeaufruf allein beweist keinen Empfang am PC.

Windows wartet bis zu zehn Minuten auf das erste gueltige Paket. Auch ein leerer
Bericht nennt jetzt Listener/Port, PC-IP-Kandidaten, empfangene/ungueltige/
gefilterte Datagramme und den Abbruchgrund. Alte BETA12.31b-Berichte mit nur
`packets=0` koennen die Fehlerstelle nicht bestimmen. Alte Logger 1.0 koennen
keine neuen Testpakete bestaetigen; bitte den mitgelieferten 1.1 benutzen.

## Betrieb und Grenzen

Die Wii sendet feste numerische Leistungs-/Fehlerzaehler, keine BIOS-/Spiel-/
Karten-/Register-/Speicherinhalte oder Dateinamen. Vier feste Queue-Plaetze;
bei Ueberlauf werden Pakete verworfen und gezaehlt. Netzwerk-IO laeuft im
Hintergrund. Die Wii akzeptiert ausschliesslich fixe ACKs von der konfigurierten
IP und dem Port mit passender Boot-/Sequenznummer; keine Fernsteuerbefehle.
UDP ist unverschluesselt und fuer das eigene lokale Netzwerk vorgesehen.

PC-Berichte trennen Build, Sitzung, Profil-Epoche, CPU-Maske, Renderer und
Experimente. CPU-Stichproben und exakte Praesentation haben eigene Nenner.
Kumulative Profilwerte verhindern doppeltes Addieren; verlorene letzte Pakete
bleiben eine Luecke. Kein automatischer FPS-Vergleich verschiedener Spielszenen.
Maximal 10.000 Pakete pro Empfang. SD-Logs und detaillierte GX-Fault-Traces
bleiben als Rueckfall erhalten. Netzwerk hat CPU-Kosten; kein Null-Overhead-
oder Wii-FPS-/Grafik-Fix behauptet.

OFF und SAVE deaktiviert den Sender; `Restore defaults` deaktiviert ihn ebenfalls.
Die Ziel-IP bleibt fuer spaetere Nutzung gespeichert. Geaenderte Menuewerte sind
bis SAVE nur Entwuerfe. Fehlgeschlagene Saves aendern den aktiven Sender nicht.
`SETUP-SD.cmd` bleibt als alternative einmalige Konfiguration kompatibel.
