# BETA12.28 – Alpha-Core aus dem Referenzabgleich

Basis `d2006f9` (Audit auf BETA12.27), Code-Batches auf separaten Branches.
Dies ist ein Entwicklungs-Teststand, keine allgemeine Alpha-Freigabe.

## Änderungen

- Normale Einstellungen auf neun Einträge gebündelt; Profile, Entwicklerseite,
  unabhängige Experimente und persistente Frontend-Werte. Gespeichertes All-OFF
  wird respektiert; 32 IDs und v1–v9 bleiben kompatibel.
- Physische CLUT-Halbwörter, CPSM-/CSM-Neuinterpretation und I8-CSA-Klemmung.
- Beide PCRTC-Kreise mit DISPLAY-Offsets und PMODE zusammengeführt; präziser
  VRAM-Fallback, bestehende GX-Einzelkreispräsentation für geeignete Fälle.
- Native PS2-Karte Port0/Slot0 mit ECC, NAND/Erase, SIO2-DMA und Persistenz.
- Standard-8-MiB-FAT und echte MCSERV-Dateioperationen für beide RPC-ABIs,
  inklusive endParam-DMA, Fehlerprüfung und offenen Dateicheckpoints.
- EE-LQ/SQ an leeren/vollen IPU-CPU-FIFOs parken ohne Daten-/Retirementverlust.

Details und gezielte Tests: [Implementierungsbatches](ALPHA-UMSETZUNG.md).
Referenzen bleiben gepinnt; keine fremden CPU-/Renderer-/FS-Backends kopiert.

## Bedienung und Rückfall

Settings enthält Renderer, Tonausgabe, HUD, Inputbudget, Zähler, CPU-Profil,
Experiments, Developer switches und Restore defaults. Alle Einträge sind per
D-Pad+A erreichbar. Experimente bleiben separat und standardmäßig OFF;
Änderungen gelten beim nächsten Cold Boot. Entwicklerseite zeigt Wunsch-
und aktive Werte mit Abhängigkeiten. Sound OFF betrifft weiter nur Ausgabe.

Frontend-Datei: `sd:/pcsx2/frontend.cfg` (versioniert). Bestehende CPU/GX-,
Chain-, PSM- und Risk-Dateien bleiben kompatibel. Pakete enthalten keine
Einstellungen und überschreiben keine Nutzerdaten.

Eine vorhandene Karte wird aus `sd:/pcsx2/memcards/MemoryCard1.ps2` geladen.
Kein automatisches Erstellen oder Formatieren. Fehlende/ungültige Images bleiben
abwesend. 8-MiB-Raw und 512+16-Spare sind möglich. Ein erfolgreicher FORMAT-RPC
ist jetzt eine echte, explizite Formatierung dieser Karte. Zusätzlicher dynamischer
Speicher: 8.650.784 Byte (rund 8,25 MiB), Checkpoint/Remount zeitweise mehr.
Allocationsfehler müssen als fehlgeschlagener Mount behandelt werden; ein
statischer MEM1-Pass beweist keinen ausreichenden gesamten MEM2-Heap.

## Reproduktion

SDK: devkitPPC r50, GCC16.1.0, libogc3.1.0, offizielles Image
`devkitpro/devkitppc@sha256:4c919aa26151dd43d88ca28c922d1fe2409579a8ba60ef56517baf1abdfb1a48`.
Frisches Quellarchiv verwenden. SPEED/DSI/AOT/Idle/GX-Flags bleiben im Script.

```sh
python3 tools/test_beta12_28_host.py --output-json /tmp/beta12-28-host.json
bash tools/build_beta12_28_alpha.sh
CHAIN_RESIDENCY=0 bash tools/build_beta12_28_alpha.sh
```

Das erste DOL enthält die Chain-Funktion mit Runtime-Standard OFF; das zweite
kompiliert sie aus. Alle drei Experimente können im ersten Build unabhängig
verglichen werden. Host-/Unicorn-Prüfungen simulieren GX-/libogc-Aufrufe.
Ausführungszählungen sind keine Wii-FPS.

## Integrierte Prüfergebnisse (17.10.2026)

- 62/62 Host-Suites bestanden, einschließlich der bisherigen 55 und sieben
  neuen Alpha-/Karten-/IPU-Suites. Einzelresultate werden im Review-ZIP geliefert.
- Vier Options-Metadatenprofile respektieren All-OFF; 16 Frontend-Kombinationen,
  alte Konfigurationsimporte und laufende/gespeicherte Werte getrennt geprüft.
- Vier Risk-Buildprofile bestanden, einschließlich unkompiliert/ohne Backend.
- Unabhängiger ausgeführter PPC-Allocator: 2.400 128-Bit-/Alias-Proben;
  echte gelinkte Wii-Allocator-Versionen: je 97 Proben mit und ohne Chain.
- Native EABI-Continuation-Oracle: 45 Budget-/Teilende-/Registerfälle bestanden;
  5.400 unabhängige Cross-Block-128-Bit-/HI/LO-/SA-/EABI-Proben, 11 Declinefälle.
- Gelinkte PPC-Zusatzprüfungen: 24 ALU-, 72 RAM-/TLB-/SMC- und 112 Sprung-
  /Delay-/IRQ-/Scheduler-Vergleiche. Alle acht Experimentkombinationen in
  16 vollständigen EE-Zustandsvergleichen, inklusive Cold-Boot-Isolation.
- Gelinkter GX-Pfad: 91 Coverage-/Z-/Alpha-Proben, 728 Alpha-Shadow-Pixel,
  vier TFX-Modi/Fog/Clamp/TEV und Risikofallbacks bestanden. Geeignetes
  synthetisches Dreieck OFF=16, ON=1 GX-Übermittlungen; GX wird simuliert,
  keine bestätigte GPU-Kanten-/RGB-/Zeitgleichheit.
- Gelinkte GS-PSM-Helfer: 53 Big-Endian-PPC-Speicherproben. XGKICK-Reject/
  Ringwrap, CSM2-Bänke, 32 Tiefentexel und IPU-Reset/Input folgen separat.
- Beide Wii-Builds mit devkitPPC erfolgreich; DOL-/ELF-Bytes stimmen jeweils
  überein. Chain: 2.078.104 Byte DOL, statisches Ende `0x815d8354`,
  2.260.140 Byte MEM1-Rest. Ohne Chain: 2.072.568 Byte DOL,
  statisches Ende `0x815b6d54`, 2.396.844 Byte MEM1-Rest.
- Keine Dolphin-/echte-Wii-Ausführung in dieser Prüfung. Statischer MEM1-Rest
  ist kein dynamischer Heap-/MEM2- oder Laufzeitstabilitätsnachweis.

DOL SHA256 (Chain):
`55f1845c07ce800c8434ba0306dc9080dacfc1e973622412c7226e9a9384b48e`

DOL SHA256 (ohne Chain):
`6dc6f8a9f8ceac1fbfd0c4f4b84a0f28c3d9e1f16cd017742d351ca078ad7da2`

Code-Buildbasis: `2e8680b6c37cc02c87e0742f020ba2dee72c833c`.
Nachfolgende Paket-/Dokumentationscommits ändern keine Emulatorsemantik.
Der Paketmanifest nennt beide SHAs, SDK, tatsächliche Compile-Flags,
leere private AOT-Tabellen sowie DOL-/ELF-Hashes.

### Gemessener Chain-Vergleich

Gleiches synthetisches 128-Instruktionsprogramm, warmer Codecache,
identischer vollständiger EE-/Timer-/IRQ-Zustand: OFF 82.467 ausgeführte
PPC-Instruktionen, ON 84.270, dabei 14 Native-Nachfolger. ON benötigt hier
rund 2,19 Prozent mehr Modellinstruktionen. Das belegt in dieser Probe keinen
Geschwindigkeitsgewinn; deshalb bleibt der Modus OFF. `compare_chain_modes.py`
reproduziert die Zählung. Speicher-/GX-Zugriffe und Wii-Zeit werden damit
nicht realistisch bewertet. Erst dieselbe Wii-Szene und identische Optionen
liefern einen Gesamt-FPS-/Zeitvergleich.

## Offene Grenzen und Freigabe

Keine neue bestätigte Crash-/Tekken-Spielbarkeit oder BIOS-Kachelkorrektur.
Vollständiges Dirty-Writeback über allgemeine JIT-Blöcke, GX-Feedback/EXTWRITE,
per-Pixel-Mipmap-/alle Blendzustände, vollständige VU-/Scheduler-/IPU-Timing-
Konformität und Soundausbau sind nicht abgeschlossen. Karten-MagicGate,
zweiter Port/Multitap, Raw-Page-/erweiterte MCSERV-RPCs und andere Geometrien
bleiben ausdrücklich unimplementiert. Beschädigte Karten werden abgewiesen,
nicht repariert. Baumvalidierung kostet CPU-Zeit; auf Wii noch nicht gemessen.

| Ziel | Neuer Code | Weiterhin fehlender Nachweis |
|---|---|---|
| SCPH-50004 OSDSYS | CLUT-/CPSM-Ansichten, PMODE-Merge | dasselbe fehlerhafte Draw und korrekte BIOS-Kacheln auf Wii |
| Crash E3 | gemeinsame GS-/IPU-/Menüpfade, bestehende Transportprüfungen | vollständige SIGNAL-/XGKICK-Quittierungsfolge und steuerbares Gameplay |
| Tekken Tag | gemeinsame Palette/Ausgabe, volle Ausgabegröße erhalten | korrekte Fragezeichen-Kacheln, Übergang in 3D-Kampf, Eingaben und FPS |
| GT3 | echte PS2-Karte und MCSERV-FAT | Karten-/Save-Abfrage im konkreten Spiel und Rennstart |
| KOF | gemeinsame Emulatorpfade | konkrete Fassung identifizieren und real testen |

[A00–A17-Arbeitsplan](ALPHA-ARBEITSPLAN.md) bleibt als langfristige Liste
bestehen. Die dortigen Hardware-Gates sind nicht durch eine neue Versionsnummer
erfüllt. Für öffentliche Spielsupport-Zusagen fehlen echte Wii-Nachweise.
