# Alpha-Abgleich: Implementierungsbatches

Basis d2006f9 / BETA12.27. Keine allgemeine Alpha-Freigabe.

## 1. Optionen und Launcher

Erzwungene GX-Testbits entfernt: gespeichertes All-OFF ist jetzt auch im
Native-GX-Build aktiv All-OFF. Getrennte Profile Wii-Basis und Interpreterreferenz,
Entwicklerseite mit gespeicherten/aktiven Werten und Abhängigkeitsgründen.
Die normale Einstellungsseite bündelt Renderer, Tonausgabe, HUD, Updatebudget,
Zähler und CPU-Profil. Alle drei Experimente sind über D-Pad+A erreichbar und
bleiben unabhängig OFF. Die 32 alten IDs und Dateien bleiben kompatibel.
HUD/Update/Zähler/Renderer werden streng versioniert in frontend.cfg gespeichert;
Restore Defaults verändert Wunschwerte, keine laufenden Nativecaches.
ARM-/IOS-Runtimefähigkeit ist weiterhin vom tatsächlich vorhandenen Dienst
abhängig; der Menütext benennt CPU-Rückfall und App-Neustart.

Tests: vier kompilierte Optionsprofile jetzt jeweils All-OFF=00000000;
16 Frontend-Konfigurationen, striktes Parsen, Backup und Profil-/Sitzungsisolation
bestanden. Bestehende 55 Host-Suites bestanden. Wii-Build folgt im integrierten
Prüfbatch; keine bisher behauptete Menü-Hardwareprüfung oder FPS-Steigerung.

## 2. Physische CLUT-Ansichten

Gemeinsame Halbwörter für CT32/CT16/CT16S, korrekte CSM1-I8-CSA-Ladung und
geklemmte letzte CT32-I8-Bank statt Ringrotation. CSM1→CSM2-I4-Ansicht und
CLD=0/6/7 behalten den temporären Speicher. Native Texturschlüssel berücksichtigen
physische Daten und letzten Lademodus. Fehlgeschlagene GPU-Synchronisierung
verändert die Palette nicht. Alte Checkpoint-Präfixe bleiben lesbar; deren
logische Ansicht bleibt bis zur ersten echten Palettenladung erhalten.

Neue 4.096 I8-CSA-Proben, Halbwort-/CPSM-/CSM-Wechsel, vorhandene Paletten-
und TEXCLUT-Fälle, GIF-Q-Checkpoint und Format-/Atlas-Fälle bestanden.
Die über die nominellen Bänke hinaus begrenzten Offset-I8-Slots folgen der
Referenzrepräsentation; kein neuer physischer PS2-Konformitätsnachweis.
BIOS-/Tekken-Kacheln bleiben ohne identisches fehlerhaftes Draw offen.

## Weitere Batches

Physische CLUT-Neuinterpretation, Zweikreis-Scanout, native PS2-Karten und
breitere IPU-/JIT-Konformität bleiben bis zu ihrem eigenen Testnachweis offen.
