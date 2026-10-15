# Gekko2 BETA12.20 – GS TEST/DATE/AFAIL import, JIT ABI audit

## Änderungen tatsächlich im Code

- `gs_gx_pipeline` erweitert um DATE, DATM, ATE, ATST, AREF und AFAIL (Standardwerte 0, damit bestehende Aufrufer unverändert bleiben).
- `gs_gx_import_texture` führt DATE (bei CT24: alle Fragmente verworfen), Source-Alpha-Test (alle acht ATST-Modi), AFAIL (KEEP/FB_ONLY/ZB_ONLY/RGB_ONLY), Z-Schreibfreigabe, FBA und FBMSK in der korrekten späten Reihenfolge aus. DATE verwirft auch die Tiefenaktualisierung. PSMCT24 behält physisch das vorhandene Alpha. RGB_ONLY behält für PSMCT32 die Ziel-Alpha-Komponente.
- Der eng begrenzte GX-Sprite-Snapshot-Pfad reicht die TEST-Register in den Import durch. Ist DATE oder ATE aktiv, werden residente GX-Ausgabe, Hardware-Blend und Hardware-Depth für diesen Pfad abgewiesen. Die CPU entscheidet die pro-Pixel-Destinationsregeln. Die komplexen Dreieckspfade und 16-Bit-Imports bleiben beim bestehenden Software-Fallback.
- Der reguläre VBlank-Schnellpfad, bestehende Blockcaches und 128-Bit-Registerresidenz *innerhalb* nativer Blöcke sind unverändert; es wurde **kein** ungesicherter PPC-Register-Handoff aktiviert.

## Host-Tests / statische Baukontrollen

- `tests/test_beta12_20_gx_alpha.c`: 159 Checks, keine Fehler (DATE CT24/CT32, alle ATE-Vergleiche, AFAIL, FBA, FBMSK, AFAIL-ZB_ONLY inklusive Z-Schreibregel).
- `tests/test_gx_pipeline_r1298.c`: 324 Fälle, keine Fehler.
- `tests/test_beta12_19_framebuffer16.c`: 16 Checks, keine Fehler.
- devkitPPC Wii-DOL gebaut und ELF/DOL-Sektionen verglichen, statisches MEM1 geprüft. Keine Ausführung auf realer Wii/Dolphin und **kein** Nachweis des Crash-E3-Gameplay-Fixes.

## Offene PPC 128-Bit-Übergabe zwischen unabhängigen Blöcken

Der Compiler `source/core/recompiler/ppc_dynarec.c` bindet in `ppc_residency_internal.h` bis zu zwölf GPR-Lanes an `r18..r29`. Im `ee_precise_block_emit` werden diese am **Eintritt und Ausgang** als callee-saved Register gesichert/wiederhergestellt; Architekturwerte werden write-through geführt. Der direkte EE-Ketten-Dispatcher `ppc_dynarec_translate_ee_cached_chain()` ruft weiterhin jede unabhängige `ee_precise_fn` per `bctrl` auf. Daraus folgt: Ein echter, persistenter PPC-Registersatz **über diese Funktionsgrenze** existiert noch nicht.

Für einen sicheren Fast-ABI-Pfad sind nötig:

1. Einheitliches gruppenbasiertes 128-Bit-Lane-Mapping über mehrere separate Blöcke, zusätzlich zum bisherigen C-kompatiblen Einstiegspunkt.
2. Spezieller native-to-native-Einstieg ohne individuelles Save/Restore, *nur* bei gleichem EE-Kontext, kompatibler Registerkarte, unverändertem GPR-/Mapping-/Code-Epoch und ausreichendem Scheduler-Budget.
3. Ein **vollständiger** Gastregister-Flush vor und nach C-Helfern, Interrupts, TLB-/EE-Exceptions, DMA- oder Interpreter-Rückkehr. Bei potentiell erfolgloser Vorbereitung dürfen keine dirty PPC-Lanes verworfen werden.
4. Source-/Mapping-/Code-Generationsvalidierung an **jeder** Kante, einschließlich RAM-Self-Modifying-Code und Cache-Eviction. Keine spekulativen Code-Daten bei Exceptions.
5. Differentialtests gegen Interpreter für sämtliche 128-Bit-Lanes (inkl. HI/LO/SA), Branch-Delay-Slots, Ausnahme-Rollback, Code-Invalidierung, Async-IOP-Events, anschließend Wii-Hardwarevergleich.

Diese spezielle ABI und damit die **vollständige blockübergreifende 128-Bit-PPC-Registerresidenz ist in BETA12.20 ausdrücklich noch nicht implementiert**. Nur den Save/Restore zu entfernen wäre ein Datenkorruptionsrisiko.

## Verbleibende Grafikfälle

Pixeltest für den eng begrenzten Sprite-GX/CPU-Import abgedeckt; residenten EFB-Shader/TEV für alle GAME-GS-Mischzustände, 16-Bit DATE/FBMSK in GX, AA1-Coverage, Framebuffer-Feedbackschleifen, dither/texture shuffles, dynamische Blendkoeffizienten und verschiedenartige GS-Alias-Layouts bleiben offen. Die BETA12.20 legt keine PS2-BIOS- oder Game-Dumps bei.
