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
Speicher: 8.651 MiB dezimal (rund 8,25 MiB), Checkpoint/Remount zeitweise mehr.
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
Ausführungszählungen sind keine Wii-FPS. Finale Prüfergebnisse und Hashes
werden nach integrierter Prüfung ergänzt.

## Offene Grenzen und Freigabe

Keine neue bestätigte Crash-/Tekken-Spielbarkeit oder BIOS-Kachelkorrektur.
Vollständiges Dirty-Writeback über allgemeine JIT-Blöcke, GX-Feedback/EXTWRITE,
per-Pixel-Mipmap-/alle Blendzustände, vollständige VU-/Scheduler-/IPU-Timing-
Konformität und Soundausbau sind nicht abgeschlossen. Karten-MagicGate,
zweiter Port/Multitap, Raw-Page-/erweiterte MCSERV-RPCs und andere Geometrien
bleiben ausdrücklich unimplementiert. Beschädigte Karten werden abgewiesen,
nicht repariert. Baumvalidierung kostet CPU-Zeit; auf Wii noch nicht gemessen.

[A00–A17-Arbeitsplan](ALPHA-ARBEITSPLAN.md) bleibt als langfristige Liste
bestehen. Die dortigen Hardware-Gates sind nicht durch eine neue Versionsnummer
erfüllt. Für öffentliche Spielsupport-Zusagen fehlen echte Wii-Nachweise.
