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

## Weitere Batches

Physische CLUT-Neuinterpretation, Zweikreis-Scanout, native PS2-Karten und
breitere IPU-/JIT-Konformität bleiben bis zu ihrem eigenen Testnachweis offen.
