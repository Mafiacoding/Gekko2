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

## 3. Zweikreis-Scanout

Geplante Ausgabe führt beide aktiven PCRTC-Kreise mit relativen DISPLAY-
Offsets, Feld-/Framehöhe, PMODE MMOD/AMOD/ALP/SLBG und BGCOLOR zusammen.
Einzelkreise mit unveränderter RGB-Semantik behalten den bisherigen GX-
Presentpfad. Die neue Komposition benutzt kohärente GS-VRAM als Software-
Fallback; GX-Zeichenbefehle bleiben aktiv. Externes Feedback/EXTWRITE und
vollständiges CRT-/Deinterlace-Timing sind weiterhin offen.

256 konstante ALP-Werte, Source-Alpha, AMOD, Hintergrund, versetzte Kreise,
Feldhöhe, Display2-Änderungserkennung und unverändertes XFB bei ungültiger
Quelle/Readbackfehler bestanden. MMOD/Alpha-Normalisierung folgt den gelesenen
PCSX2-Mergepfaden, mit expliziter Integer-Rundung; keine behauptete physische
PS2-/Wii-Pixelgleichheit. Einzelkreis-Format-/Skalierungsregressionen bestanden.

## 4. Native PS2-Kartenbasis und SIO2-DMA

Neues eigenständiges Kartengerät mit 512+16-Byte-Seiten, 16-Seiten-Erase,
Probe/GetSpec, Terminator, Sektorwahl, NAND-Read/Write/Erase, öffentlichem
Reset-Handshake, Daten-XOR und ECC-Parität. Raw-8-MiB und 8,650,752-Byte-
Images mit Sparebytes werden unterstützt. Kein automatisches Erstellen,
Formatieren oder Einfügen einer Karte. Vorhandenes
`sd:/pcsx2/memcards/MemoryCard1.ps2` wird vor Gaststart für Port 0/Slot 0 geladen.
Die Datei bleibt außerhalb aller öffentlichen Pakete. Änderungen werden bei
Pause/Cold Boot/HBC über Temp/Backup gesichert; Fehler behalten die dirty Daten.
Eine eingelegte Karte benötigt zusätzlich rund 8,65 MiB dynamischen Speicher.

SEND3 trennt Kommando-/Antwortlängen und DMA-Blockpadding. IOP-DMA11/12
überträgt akzeptierte Bytes, hält leere Antworten aus und signalisiert nur
vollständige Transfers. SIO2 meldet synchron IRQ17; keine exakte Wire-Latenz.
PS1-Pad/Karte bleiben erhalten. GETINFO liefert echten Typ sowie validierte
Superblock-/FAT-Format-/Free-Metadaten. Nicht implementierte HLE-Dateioperationen
geben Fehler statt erfundener erfolgreicher Format-/Save-/Close-Antworten.

Checkpoint P2MC speichert Inhalt, Cursor und Modus und bindet eine vorhandene
passende Kartenkonfiguration. In-flight SIO2-Kartencheckpoints werden ausdrücklich
abgewiesen; die vollständige Serialisierung aller FIFO-/DMA-Zwischenzustände
ist offen. MagicGate F0, Multitap/zweiter Kartenport und vollständige
MCSERV-Dateisystemoperationen bleiben offen. Daher kein fertiger Spielstand-
oder GT3-/BIOS-Kartennachweis.

Tests: 1.024 unabhängige Einzelbit-ECC-Proben, Daten-/Erase-/Remount-Persistenz,
Readonly-/Fehlerfälle, synthetische Superblock-/FAT-Metadaten, zwei echte gepaddete
SIO2-DMA-Frames und wartende Antwort-DMA. GETINFO-DMA, vollständiger Karten-
Checkpoint und Ablehnung laufender Serialtransfers bestanden. PS1-/Pad-/alte
Checkpoint-Regressionen bestanden. Die alte CLOSE-Fixture verlangte Erfolg
für einen nie geöffneten Descriptor; sie prüft jetzt den expliziten Fehler.
Zwei alte Whitebox-Fixtures haben ihren fehlenden system.h-Include erhalten.

## 5. Präzise EE-IPU-FIFO-Retries

EE-LQ/SQ warteten bisher nicht vor leerem/vollen IPU-FIFO: leere Reads
publizierten Nullen, volle CPU-Writes wurden verworfen. Der skalare Fetch-
Frontend prüft jetzt vor Native-/AOT-Zulassung die tatsächlich adressierten,
auf 16 Byte gerundeten FIFO-Ports. Bei Rückstau bleiben PC/next_pc, Delay-Slot,
Retirement und 128-Bit-Ziel unverändert; nur reale Park-/Gerätearbeit läuft.
Nach Freigabe wird genau ein Qword übertragen. LQ r0 behält seinen No-Access-
Vertrag. Die FIFO-API meldet Stau explizit (-1) und verändert keine Lesepuffer.

200 gerichtete LQ/SQ-Retries mit Kernelalias/unaligniertem EA, Delay Slot,
Generation des Eingabefreiraums durch FDEC, einmaligem Output-Verbrauch und
LQ r0 bestanden. IPU-DMA-Stall, 384 IDEC-/399.764 Feldprüfungen, vorhandene
GIF/SQ-/FMAC-/DMA-Retries und EE-Exceptions bestanden. Dies ist eine
unabhängig reproduzierte Datenverlustkorrektur, kein Crash-E3-Fixbeleg.

## 6. Standard-PS2-FAT und MCSERV-Dateien

Die vorherigen nicht implementierten Dateiantworten sind für Standard-8-MiB-
PS2-Karten durch echte FAT-/Verzeichnisoperationen ersetzt: OPEN, CLOSE, READ,
WRITE mit libmc-Präfix, SEEK, FLUSH, GETDIR mit Wildcards/Fortsetzung, CHDIR,
SETINFO einschließlich Umbenennen, DELETE, explizites FORMAT/UNFORMAT. Beide
RPC-Nummernräume werden getrennt zugeordnet; READ benutzt die jeweiligen
16-/64-Byte-Anfangs-/Endpuffer und publiziert sie über endParam vor REND.
DMA-Bereiche werden vor Datei-/Positionsänderung begrenzt, JIT-Codeänderungen
gemeldet. Standardmäßig wird weiterhin keine Karte erstellt oder formatiert.

Geometrie/IFC/FAT, Schleifen, Dateilängen, Elternzeiger und Cluster-Crosslinks
werden vor schreibendem Dateizugriff geprüft. Platzmangel verändert eine
bestehende Datei nicht. Löcher werden genullt; ECC wird für betroffene Seiten
erneuert. FS-Handles, Position, cwd und Verzeichnisfortsetzung sind im optionalen
MCFS-Checkpoint enthalten. Kartenwechsel, physischer SIO2-Write und Cold Boot
verwerfen alte HLE-Handles. Andere Geometrien, Verzeichnis-Dateidescriptoren,
raw-page-RPCs/GETENTSPACE/Erweiterungen, MagicGate, zweiter Port/Multitap und
defekte Karten-Reparatur sind ausdrücklich nicht implementiert.

Referenz: PS2SDK `329e2bb43287e16af16c4dde1c614e2662c24468`,
`mcman/src/main.c`, `ps2mc_fio.c`, `mcman-internal.h`, `mcserv/src/mcserv.c`
und `libmc-common.h` (AFL-2.0). Unabhängige Implementierung der beschriebenen
Strukturen/Protokolle; kein importierter Dateisystem-Backend. Wichtig:
libmc O_CREAT ersetzt vorhandene Dateien; das ist kein POSIX-O_CREAT.
Nicht-root-Verzeichnisse halten ihre Länge im Eltern-Eintrag.

Tests: 512 READ-ABI/Ausrichtungs/EOF-Fälle, WRITE-Präfix, beide Opcode-Maps,
benannte RPCs, persistente Dateien, Sparse-Seek, Rename/Enumeration, Schreibschutz,
unabhängig angelegtes fragmentiertes Kartenlayout, Schleifen/Crosslinks und
Checkpoint mit offenem Dateihandle bestanden. Der Hersteller-ROM-/Spielbetrieb
und Austausch mit physischer PS2/Wii sind nicht getestet. Die Admissonsprüfung
scannt den Verzeichnisbaum; Kosten auf Wii noch nicht gemessen.

## Offene Nachweise

Die konkreten Abgleichkorrekturen 1–6 sind implementiert; vollständige PS2-
Hardwarekonformität, allgemeines Dirty-JIT-Writeback, GX-Feedback/EXTWRITE und
hardwarebestätigte Crash-/Tekken-/BIOS-Kachelkorrekturen bleiben offen.
