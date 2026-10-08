# R1331: CPU/GX-Optionen und sechs Optimierungsbatches

Stand: 8. Oktober 2026. Grundlage ist der vom Nutzer getestete R1330-L-Stand.
Wii hat Vorrang. Die Architekturprüfungen sind keine Messung echter Wii-FPS.

Verifiziert: 21 gezielte PPC-Suiten und 64 native GS-Tests; separate
Ownership-/Arena-Tests mit ASan/UBSan. Der positive RAM-Cache bewahrt in
192 gemischten Programmen denselben Gastzustand mit OFF/ON. In einer
Warmsequenz mit acht LW-Instruktionen ergeben sich 5052/5247 modellierte
PPC-Instruktionen bei TLB-Eintrag 1 (OFF/ON), 9836/5247 bei Eintrag 47.
Das belegt einen Vorteil bei längeren Suchen und einen Nachteil bei kurzen;
keine der Zahlen ist eine Wii-Zeit- oder FPS-Messung.

## Optionen bedienen

Im Launcher **SETTINGS → DOWN → CPU / GX OPTIMIZATIONS** öffnen.
Mit UP/DOWN auswählen, mit A oder LEFT/RIGHT umschalten, mit B zurück.
Die Auswahl wird als `sd:/pcsx2/optimization.cfg` gespeichert. Änderungen
gelten beim nächsten **Kaltstart mit A auf BIOS / OSDSYS oder START DISC**.
START setzt die pausierte Sitzung mit ihren bisherigen Optionen fort.
So bleibt ein bereits erzeugter Codeblock während seiner gesamten Lebensdauer
an dieselben Übersetzungsregeln gebunden. Keine Option verschiebt oder löscht
gerade ausgeführten Code.

Für GX zusätzlich auf der normalen Settings-Seite **GX (EXPERIMENTAL): ON**
setzen. Die Unteroptionen setzen diesen Ausgabeschalter voraus. `ready=1`
meldet die Initialisierung; aktive Arbeit belegen erst Draw-, Residency- und
Readback-Zähler. Ein zusätzliches Grafik-API ist für diese Pfade nicht nötig:
GIF/GS-Zustände werden über die vorhandene GX-Backend-Schnittstelle umgesetzt.

| Option | Tatsächliche Wirkung | Standard im Options-Build |
|---|---|---|
| EE dynarec | Unterstützte EE-Berechnungen verwenden PPC-Code; übrige Interpreter | ON |
| IOP dynarec | Unterstützte IOP-Berechnungen und Blöcke verwenden PPC | ON |
| VU0 / VU1 dynarec | Unterstützte VU-Paare/Blöcke verwenden PPC, Scheduler bleibt zuständig | ON |
| EE native blocks | Bis zu acht EE-Instruktionen pro nativer Ausführung mit echten Grenzen | ON |
| Native block links | Bewachte native Nachfolger innerhalb des EE-Budgets | ON |
| GPR residency | Wiederholt benutzte GPR-Wörter bleiben in gesicherten PPC-Registern | ON |
| 6 MiB code arena | Gemeinsame Codeverwaltung für EE, IOP und VU | ON |
| GX resident depth / blend | Zulässige RGB-/Blend-Pfade bleiben im EFB; exakte GS-Metadaten bleiben erhalten | ON |
| GX texture reuse | Wiederverwendung des vorhandenen Textursnapshots bei gültigem Schlüssel | ON |
| Fastmem RAM page cache | Experimenteller positiver Cache für geprüfte RAM-TLB-Übersetzungen | **OFF** |
| GX independent VRAM writes | Begrenzte unabhängige CPU-Schreibzugriffe lösen keinen RGB-Readback aus | ON |
| Native load / store reduction | Entfernt redundante GPR-Zugriffe innerhalb nativer Instruktionskörper | ON |
| GX Gouraud shading | Vorhandener nativer Farbinterpolationspfad für geeignete Dreiecke | ON |

OFF bei einem CPU-Dynarec verhindert dessen native Dispatches. OFF bei einer
GX-Unteroption verwendet ihren bestehenden korrekten Ersatzpfad. Abhängigkeiten
bleiben erhalten: Registerhaltung und Blocklinks benötigen tatsächlich erzeugte
Blöcke; ein Schalter allein erzeugt keine Beschleunigung.

## Batchliste

### 1. Codearena und Caches

**Implementiert:** 6-MiB-Arena mit 32-Byte-Ausrichtung, geteilt von EE/IOP/VU.
Code wird direkt an seiner endgültigen Adresse erzeugt. Finalisierung gibt
ungenutzten Reservierungsraum frei, ohne Code zu verschieben. Freie Bereiche
werden zusammengeführt. Besitzer geben Code erst nach dem nativen Aufruf frei.
Bei Speichermangel bleibt der Interpreter-Ersatz erhalten; es gibt keinen
ungeprüften Sprung und kein globales Löschen noch benutzter Codebereiche.
DC-Flush und IC-Invalidierung bleiben bei jeder Finalisierung erhalten.

Die Arena reserviert 6 MiB MEM1 auch dann, wenn die Laufzeitoption OFF ist.
OFF verwendet den bisherigen Heap-Pfad. Der separate Reference-Build definiert
`GEKKO2_CODE_ARENA_DISABLE` und reserviert diese 6 MiB überhaupt nicht.
Bestehende PC-/Encoding-, Negativ-, Mapping- und Code-Seitencaches werden
weiter genutzt. Kapazität, Nutzung, Maximum und Fehlversuche stehen im Log.

**Weitere Arbeit:** Konflikt-/Lebensdauerprofile auf Wii auswerten, bevor
Tag-Caches vergrößert oder eine andere Verdrängungsstrategie eingeführt wird.

### 2. Registerhaltung und Übergaben

**Implementiert bzw. jetzt auswählbar:** bestehende GPR-Registerhaltung und
Intra-Body-Registerallokation. Kanonischer Gastzustand bleibt an jeder
beobachtbaren Grenze erhalten. Fremde Kontextänderungen und Generationen
werden weiterhin geprüft; Helfer dürfen keinen veralteten Registerwert sehen.

**Noch offen:** anhaltendes FPU-Pinning und weitere selektive Übergaben über
Interpreter-/HLE-Fallbacks. nullDCs SH4-FPU-Pinning ist keine fertige R5900-
Implementierung. PS2-Rohbits, NaN-/Denormalbehandlung, ACC und VU-Zustand müssen
zuerst mit unabhängigen Orakeln nachgewiesen werden. R1331 behauptet diese
Mechanismen deshalb nicht als umgesetzt.

### 3. Blockübergänge und Zielcaches

**Implementiert bzw. aktiviert:** eigener EE-Blockschalter unabhängig vom alten
`PCSX2WII_FAST`-Sammelschalter, bewachte native Nachfolger und bestehender
cachezeilengroßer Dispatch-Tag. Originales EE/IOP-Verhältnis bleibt 8:1.
Quellcodeänderung, Mapping/ASID, Cache-Seriennummer, Delay-Slot, Ausnahme,
Interrupt und verbleibendes Budget bleiben Grenzen der Weiterleitung.

**Noch offen:** direktes Patchen von PPC-Zweigzielen und größere mehrwegige
indirekte Zielcaches. Dafür braucht die Codeverdrängung zuerst vollständige
Rückverweise auf jede eingehende Verbindung. Die jetzigen geprüften
Funktionsverbindungen verhindern Sprünge in freigegebenen Code.

### 4. GX-Renderziele, Grafikzustände und Texturen

**Implementiert bzw. auswählbar:** vorhandene residente RGB-Renderziele,
zulässige GX-Blendprogramme, native Geometrie und Textursnapshot-Reuse.
Exakte GS-Alpha-/Z-Daten werden weiterhin über die vorhandenen Metadaten/
CPU-Schattenpfade gepflegt. Unsupported State geht in einen überprüften
Fallback. Vollständiges GS-Alpha und alle Z-Formate passen nicht automatisch
in das verwendete RGB8-EFB.

**Neu:** Farbschreibzugriffe, 16-Bit-/Indexformate und Span-Fills außerhalb des
konservativ geschützten Renderzielbereichs lassen dessen GPU-Besitz bestehen.
Ein Alias, Raw-Pointer-Zugriff oder fehlgeschlagener Resolver wird weiter
synchronisiert bzw. verweigert. Ein Resolver mit Range-Guard darf nur seine
geschützten Bytes verändern. Dadurch können unabhängige Texturuploads ein
aktives Farbziel nicht mehr allein wegen eines globalen Schreibbarriers
zurück auf die CPU ziehen.

**Noch offen:** mehrere persistente Texturen/Renderziele, adaptive Zielhöhe
und native Gouraud-Pfade für zusätzliche GS-Zustände. Der existierende
Texturcache hält einen Snapshot, keine allgemeine Mehrfach-Texturverwaltung.

### 5. Verteilung Interpreter / Dynarec / Software / GX

**Implementiert:** getrennte EE-, IOP-, VU- und GX-Optionen, Kaltstart-Apply,
Persistenz und tatsächliche Routen-/Arbeitszähler. Der vorhandene Gouraud-Pfad
kann separat verglichen werden. Unterstützte CPU-Berechnungen werden auf
PPC ausgeführt; geeignete Rasterarbeit läuft auf GX.

**Weitere Arbeit:** häufige Fallbacks anhand der neuen Logs priorisieren und
Opcode-/GS-Zustandsgruppen mit eigener Verifikation ergänzen. Ein Interpreter-
Aufruf ist nicht automatisch ein Gewinnkandidat: bei billigen Befehlen kann
die Übergabe an einen kleinen JIT-Stub teurer sein als die C-Berechnung.
VU0/VU1 gehören zur CPU-/Vektoremulation; ihre Ergebnisse erreichen GX über
GIF/GS. Sie werden nicht als separate VU-Prozessoren in GX eingebunden.

### 6. Fastmem

**Implementierter erster Schritt:** positiver 128-Einträge-Cache für normale
RAM-TLB-Seiten. Schlüssel: Zustand, Mapping-Epoche, virtuelle 4-KiB-Seite und
ASID. Auch bei Hits werden der live ausgewählte TLB-Tupel und RAM-Grenzen
geprüft; EntryLo/D bleibt für jeden Store relevant. Große PS2-Seiten werden
korrekt in 4-KiB-Unterseiten zwischengespeichert. Fehler, ROM, MMIO und
Scratchpad werden nicht zu normalen RAM-Zugriffen gecacht. Architekturpfade
TLBWI/TLBWR/EntryHi/Checkpoint invalidieren über die vorhandene Mapping-
Benachrichtigung. Externe direkte Änderungen an TLB-Einträgen müssen ebenfalls
`ee_jit_notify_mapping_change()` aufrufen, insbesondere bei neuer Priorität
überlappender Einträge. Unterstützte native RAM-Befehle behalten ihre direkten,
bytevertauschten PPC-Lese-/Schreibzugriffe und die Quellcode-Invalidierung.

**Noch offen:** nullDC-artige PPC-MMU-/DSI-Fault-Abbildung und gepatchte
Cold-Helper. Dafür sind Wii-Exception-Integration, PS2-TLB/ASID, gleichzeitige
Aliase, Schreibschutz, MMIO und sichere Wiederaufnahme zu implementieren und
auf Hardware zu testen. Der neue Cache ist ausdrücklich kein Hardware-MMU-
Fastmem. Er ist standardmäßig OFF und darf erst nach erfolgreichem
Wii-Boot-/Logvergleich als Standard diskutiert werden.

## Nachweise und Messung

`tools/verify_r1331.py ELF --nm /path/powerpc-eabi-nm --output /path/results`
prüft die tatsächlichen verlinkten PPC-CPU-Pfade. GX und Plattformdienste
werden dabei modelliert; FIFO/Reihenfolge und Gastzustand werden geprüft.
Die native GS-Suite läuft mit `tools/verify_native_gs_r1330l.py`.
Arena und VRAM-Ownership haben zusätzliche ASan/UBSan-Grenzfalltests.

Neue Logzeilen: `BUILD mask/next_boot_mask`, `CODE_ARENA`, `FASTMEM`,
`OPT_ROUTES`; vorhandene `EE_BLOCK`, `IOP_BLOCK`, `VU_JIT`, `WORD_ALLOC`,
`RESIDENT`, `GX_WORK` und `GX_RESIDENT_PIPELINE` bleiben erhalten.
Vergleiche pro Option denselben BIOS, denselben EE-Bereich und dieselben
GX-Einstellungen. Output-FPS sind nicht gleich neue emulierte GS-Frames.
Ein messbarer Wii-FPS-Gewinn oder ein beseitigter Hardware-Crash ist ohne
neuen Wii-Test noch nicht belegt.
