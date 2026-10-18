# Gekko2 – Gesamtinventar und Umsetzungsstand

Basis BETA12.28 / `7681af2`. Diese Liste führt bestätigte Lücken, Teilpfade,
absichtliche skalare Sicherheitsgrenzen und fehlende Hardwarebelege getrennt.
Sie ist kein Beweis vollständiger PS2-Spezifikationsabdeckung. Alle erfassten
Laufzeitquellen und rohe Reviewtreffer stehen im maschinenlesbaren Audit.
Kommentar-Treffer können historische Fixes oder zulässige Rückfälle bezeichnen.
Native Akzeptanz, tatsächliche PPC-Ausführung und Wii-Messung sind getrennte Gates.

Reproduktion: `python3 tools/audit_completion.py --output /tmp/completion.json`.
Vorhandene MMI-, VU0-Macro- und VU-Upper-Audits zusätzlich ausführen.
Öffentliche AOT-Tabellen sind absichtlich leer; das bedeutet keine fehlende ISA.

| Status | Anzahl |
|---|---:|
| hardware_nachweis | 10 |
| implementiert_getestet | 6 |
| offen | 8 |
| optional_abwesend | 3 |
| skalar_absicht | 1 |
| teilweise | 40 |
| vorhanden_pruefen | 13 |

| ID | Befund / Aufgabe | Status | Code / Abnahme |
|---|---|---|---|
| EE-01 | C.F.S im Interpreter, Single-/Block-Emitter und AOT-Klassifizierer | implementiert_getestet | [source/core/ee/ee_core.c](../source/core/ee/ee_core.c); RI, FPU/COP2, Reserved und Decoder-/PPC-Zustände mit unabhängigen Oracles |
| EE-02 | BC2F/T/FL/TL: native Single-Fragmente, weiter konservative Blockgrenze | implementiert_getestet | [source/core/ee/ee_core.c](../source/core/ee/ee_core.c); RI, FPU/COP2, Reserved und Decoder-/PPC-Zustände mit unabhängigen Oracles |
| EE-03 | Unbekannte EE-Dispatchfälle liefern RI statt Emulator-Halt | implementiert_getestet | [source/core/ee/ee_core.c](../source/core/ee/ee_core.c); RI, FPU/COP2, Reserved und Decoder-/PPC-Zustände mit unabhängigen Oracles |
| EE-04 | FPU-Flags und PS2-Mantissen-/Denormalmodell | teilweise | [source/core/ee/ee_core.c](../source/core/ee/ee_core.c); RI, FPU/COP2, Reserved und Decoder-/PPC-Zustände mit unabhängigen Oracles |
| EE-05 | COP0/TLB/ASID/ERET/EI/DI bleiben Systembarrieren | skalar_absicht | [source/core/ee/ee_core.c](../source/core/ee/ee_core.c); RI, FPU/COP2, Reserved und Decoder-/PPC-Zustände mit unabhängigen Oracles |
| EE-06 | CACHE/SYNC/PREF bilden nur bisherige Baseline-Semantik ab | teilweise | [source/core/ee/ee_core.c](../source/core/ee/ee_core.c); RI, FPU/COP2, Reserved und Decoder-/PPC-Zustände mit unabhängigen Oracles |
| JIT-01 | 103 MMI-Formen haben Emitter und Blockzulassung | vorhanden_pruefen | [source/core/recompiler/ee_jit.c](../source/core/recompiler/ee_jit.c); Produktive Zulassung, alle Exits, Guards, EABI und ausgeführte PPC-Differentialtests |
| JIT-02 | Merge-/COP1-/COP2-RAM wird im Block statt Single-Emitter umgesetzt | vorhanden_pruefen | [source/core/recompiler/ee_jit.c](../source/core/recompiler/ee_jit.c); Produktive Zulassung, alle Exits, Guards, EABI und ausgeführte PPC-Differentialtests |
| JIT-03 | Allgemeines Dirty-Writeback über Blockgrenzen fehlt | teilweise | [source/core/recompiler/ee_jit.c](../source/core/recompiler/ee_jit.c); Produktive Zulassung, alle Exits, Guards, EABI und ausgeführte PPC-Differentialtests |
| JIT-04 | Gemeinsames Residenz-Mapping ist gruppenweise begrenzt | teilweise | [source/core/recompiler/ee_jit.c](../source/core/recompiler/ee_jit.c); Produktive Zulassung, alle Exits, Guards, EABI und ausgeführte PPC-Differentialtests |
| JIT-05 | Code-/DMA-/TLB-/Budget-/IRQ-Kantenguards erhalten und verbreitern | vorhanden_pruefen | [source/core/recompiler/ee_jit.c](../source/core/recompiler/ee_jit.c); Produktive Zulassung, alle Exits, Guards, EABI und ausgeführte PPC-Differentialtests |
| JIT-06 | Unabhängige legale Opcode-/Operand-Abdeckung vervollständigen | offen | [source/core/recompiler/ee_jit.c](../source/core/recompiler/ee_jit.c); Produktive Zulassung, alle Exits, Guards, EABI und ausgeführte PPC-Differentialtests |
| JIT-07 | AOT-Infrastruktur vorhanden, öffentliche Tabellen absichtlich leer | vorhanden_pruefen | [source/core/recompiler/ee_jit.c](../source/core/recompiler/ee_jit.c); Produktive Zulassung, alle Exits, Guards, EABI und ausgeführte PPC-Differentialtests |
| JIT-08 | Chain liefert bisher keinen gemessenen Wii-Gesamtgewinn | hardware_nachweis | [source/core/recompiler/ee_jit.c](../source/core/recompiler/ee_jit.c); Produktive Zulassung, alle Exits, Guards, EABI und ausgeführte PPC-Differentialtests |
| IOP-01 | Load-Delay und präzise Integer-Exceptions bereits vorhanden | vorhanden_pruefen | [source/core/iop/iop_core.c](../source/core/iop/iop_core.c); R3000-Load-Delay/Exceptions, Native/HLE/IRQ und Memory-Notify |
| IOP-02 | Native IOP-/Lightrec-Zulassung und HLE-Interlocks umfassend prüfen | teilweise | [source/core/iop/iop_core.c](../source/core/iop/iop_core.c); R3000-Load-Delay/Exceptions, Native/HLE/IRQ und Memory-Notify |
| IOP-03 | Optionale PS1-Kompatibilität/GTE-Geräte | optional_abwesend | [source/core/iop/iop_core.c](../source/core/iop/iop_core.c); R3000-Load-Delay/Exceptions, Native/HLE/IRQ und Memory-Notify |
| VU-01 | 95 Upper-Emitter und viele VU0-Macro-Formen bereits vorhanden | vorhanden_pruefen | [source/hw/vu.c](../source/hw/vu.c); Getaktete Paare, Flags/ACC/VI, Q/P/EFU und unabhängige Float-Oracles |
| VU-02 | Getaktete VU-Arithmetik nutzt gemeinsame skalare/AOT-Helfer | teilweise | [source/hw/vu.c](../source/hw/vu.c); Getaktete Paare, Flags/ACC/VI, Q/P/EFU und unabhängige Float-Oracles |
| VU-03 | PS2-Float/ACC-Zwischenwerte und EFU-Präzision | teilweise | [source/hw/vu.c](../source/hw/vu.c); Getaktete Paare, Flags/ACC/VI, Q/P/EFU und unabhängige Float-Oracles |
| VU-04 | CLIP-Latenz und VI-/Branch-Forwarding | teilweise | [source/hw/vu.c](../source/hw/vu.c); Getaktete Paare, Flags/ACC/VI, Q/P/EFU und unabhängige Float-Oracles |
| VU-05 | Q/P, FMAC-Queue, M-Yield und Scoreboards vorhanden | vorhanden_pruefen | [source/hw/vu.c](../source/hw/vu.c); Getaktete Paare, Flags/ACC/VI, Q/P/EFU und unabhängige Float-Oracles |
| GIF-01 | Crash-SIGNAL-/XGKICK-Stau ohne vollständigen Fixbeleg | hardware_nachweis | [source/hw/gif.c](../source/hw/gif.c); Producer/ACK, Pfadbesitz, EOP, Ringwrap und Backpressure ohne Datenverlust |
| GIF-02 | PATH1/2/3, IMT/MSKPATH3/DIRECTHL und FIFO-Gesamtmatrix | teilweise | [source/hw/gif.c](../source/hw/gif.c); Producer/ACK, Pfadbesitz, EOP, Ringwrap und Backpressure ohne Datenverlust |
| VIF-01 | UNPACK/STCYCL/Masken/TOP/ITOP/MPG-Gesamtmatrix | teilweise | [source/hw/vif.c](../source/hw/vif.c); UNPACK-/STCYCL-/Maskenmatrix und laufende VU/DMA-/IRQ-Unterbrechungen |
| VIF-02 | FLUSH/MSCAL/MSCNT bei laufender VU und XGKICK | teilweise | [source/hw/vif.c](../source/hw/vif.c); UNPACK-/STCYCL-/Maskenmatrix und laufende VU/DMA-/IRQ-Unterbrechungen |
| DMA-01 | Reverse-VIF1/GS-Chain und Richtungs-Sonderfälle | teilweise | [source/hw/dma.c](../source/hw/dma.c); Tagarten, Richtungen, Rest-QWC, FIFO, Fristen und tatsächliche IRQs |
| DMA-02 | Interleave/MFIFO/REFS und Bus-Timing | teilweise | [source/hw/dma.c](../source/hw/dma.c); Tagarten, Richtungen, Rest-QWC, FIFO, Fristen und tatsächliche IRQs |
| TIME-01 | EE/IOP-Scheduler basiert weiter auf Instruktionsslots | teilweise | [source/core/system.c](../source/core/system.c); Unveränderte sichtbare Geräteereignisse und belegte Zyklusrelationen |
| TIME-02 | Physische Bus-/FIFO-/IRQ-Latenzen teilweise synchron | teilweise | [source/core/system.c](../source/core/system.c); Unveränderte sichtbare Geräteereignisse und belegte Zyklusrelationen |
| GS-01 | 13 PSM-Quellformate und physische CLUT implementiert | vorhanden_pruefen | [source/hw/gif.c](../source/hw/gif.c); Unabhängige Pixel-/Adress-/Transfer-/Displayoracles vor GX-Zulassung |
| GS-02 | Per-Pixel-LOD statt pauschaler Primitive-LOD | teilweise | [source/hw/gif.c](../source/hw/gif.c); Unabhängige Pixel-/Adress-/Transfer-/Displayoracles vor GX-Zulassung |
| GS-03 | MTBA/MIPTBP, Mipbreiten und trilineare Randfälle | teilweise | [source/hw/gif.c](../source/hw/gif.c); Unabhängige Pixel-/Adress-/Transfer-/Displayoracles vor GX-Zulassung |
| GS-04 | AA1 und exakte Linien-/Dreieckskanten | teilweise | [source/hw/gif.c](../source/hw/gif.c); Unabhängige Pixel-/Adress-/Transfer-/Displayoracles vor GX-Zulassung |
| GS-05 | COLCLAMP/PABE/ALPHA/FBA/FBMSK-Gesamtmatrix | teilweise | [source/hw/gif.c](../source/hw/gif.c); Unabhängige Pixel-/Adress-/Transfer-/Displayoracles vor GX-Zulassung |
| GS-06 | Texture-Shuffle/Framebuffer-Feedback | teilweise | [source/hw/gif.c](../source/hw/gif.c); Unabhängige Pixel-/Adress-/Transfer-/Displayoracles vor GX-Zulassung |
| GS-07 | Zweikreis-PMODE-Komposition ist implementiert | vorhanden_pruefen | [source/hw/gif.c](../source/hw/gif.c); Unabhängige Pixel-/Adress-/Transfer-/Displayoracles vor GX-Zulassung |
| GS-08 | EXTWRITE/externe PCRTC-Feedback-Ausgabe | offen | [source/hw/gif.c](../source/hw/gif.c); Unabhängige Pixel-/Adress-/Transfer-/Displayoracles vor GX-Zulassung |
| GS-09 | CRT-/Interlace-/Field-Timing und Präsentation | teilweise | [source/hw/gif.c](../source/hw/gif.c); Unabhängige Pixel-/Adress-/Transfer-/Displayoracles vor GX-Zulassung |
| GX-01 | Komplexe GS-Zustände fallen auf Software zurück | teilweise | [source/hw/gs_gx.c](../source/hw/gs_gx.c); Softwarevergleich, Ownership/Fences und tatsächliches GPU-Readback |
| GX-02 | CT16/16S und maskierte Ziel-/Depth-Blends | teilweise | [source/hw/gs_gx.c](../source/hw/gs_gx.c); Softwarevergleich, Ownership/Fences und tatsächliches GPU-Readback |
| GX-03 | CPU-Pixelberechtigungen bleiben trotz GX-RGB teuer | teilweise | [source/hw/gs_gx.c](../source/hw/gs_gx.c); Softwarevergleich, Ownership/Fences und tatsächliches GPU-Readback |
| GX-04 | Cache-/Draw-Bündelung und partielle Readbacks | teilweise | [source/hw/gs_gx.c](../source/hw/gs_gx.c); Softwarevergleich, Ownership/Fences und tatsächliches GPU-Readback |
| GX-05 | Echte TEV-/Blend-/Kantenpixel nicht durch Hostmodelle bewiesen | hardware_nachweis | [source/hw/gs_gx.c](../source/hw/gs_gx.c); Softwarevergleich, Ownership/Fences und tatsächliches GPU-Readback |
| IPU-01 | Legale Hauptcommands haben Implementierungen; Reserved separat führen | vorhanden_pruefen | [source/hw/ipu.c](../source/hw/ipu.c); Legale unabhängige Bitstreams, Rundung, Retry, Reset und Gerätefristen |
| IPU-02 | MPEG-Koeffizienten/Escape/Field/Scan-Konformität | teilweise | [source/hw/ipu.c](../source/hw/ipu.c); Legale unabhängige Bitstreams, Rundung, Retry, Reset und Gerätefristen |
| IPU-03 | IDCT-/CSC-/PACK-Grenzen und Rundung | teilweise | [source/hw/ipu.c](../source/hw/ipu.c); Legale unabhängige Bitstreams, Rundung, Retry, Reset und Gerätefristen |
| IPU-04 | CPU-/DMA-FIFO-Retries implementiert | vorhanden_pruefen | [source/hw/ipu.c](../source/hw/ipu.c); Legale unabhängige Bitstreams, Rundung, Retry, Reset und Gerätefristen |
| IPU-05 | Physische IPU-/DMA-/BUSY-Latenzen | teilweise | [source/hw/ipu.c](../source/hw/ipu.c); Legale unabhängige Bitstreams, Rundung, Retry, Reset und Gerätefristen |
| SPU-01 | Noise-Generator, alle 64 Clockwerte und NON im Mixer angebunden | implementiert_getestet | [source/hw/spu2_mixer.c](../source/hw/spu2_mixer.c); Unabhängige Clock-/PCM-/Register-Oracles, IRQ/DMA und Checkpoint |
| SPU-02 | Pitch-Modulation aus voriger Post-ADSR-Stimme; Stimme 0 ausgenommen | implementiert_getestet | [source/hw/spu2_mixer.c](../source/hw/spu2_mixer.c); Unabhängige Clock-/PCM-/Register-Oracles, IRQ/DMA und Checkpoint |
| SPU-03 | Voice-/Master-Sweeps, VOLX und abwärtskompatibler Checkpoint | implementiert_getestet | [source/hw/spu2_mixer.c](../source/hw/spu2_mixer.c); Unabhängige Clock-/PCM-/Register-Oracles, IRQ/DMA und Checkpoint |
| SPU-04 | Gaussian-Interpolation fehlt | offen | [source/hw/spu2_mixer.c](../source/hw/spu2_mixer.c); Unabhängige Clock-/PCM-/Register-Oracles, IRQ/DMA und Checkpoint |
| SPU-05 | Reverb-/Effekt-RAM-Datapath fehlt | offen | [source/hw/spu2_mixer.c](../source/hw/spu2_mixer.c); Unabhängige Clock-/PCM-/Register-Oracles, IRQ/DMA und Checkpoint |
| SPU-06 | ADSR/ENVX/Key-On-Timing und IRQ-wirksame stille Stimmen | teilweise | [source/hw/spu2_mixer.c](../source/hw/spu2_mixer.c); Unabhängige Clock-/PCM-/Register-Oracles, IRQ/DMA und Checkpoint |
| SPU-07 | SPU2-DMA/AutoDMA/Capture und Ausgabe angebunden | vorhanden_pruefen | [source/hw/spu2_mixer.c](../source/hw/spu2_mixer.c); Unabhängige Clock-/PCM-/Register-Oracles, IRQ/DMA und Checkpoint |
| MC-01 | Native PS2-Karte und Standard-FAT-Dateien implementiert | vorhanden_pruefen | [source/hw/ps2_mc_files.c](../source/hw/ps2_mc_files.c); Exakte RPC-/Karten-Layouts, Persistenz, Schreibschutz und keine Datenkorruption |
| MC-02 | GETENTSPACE implementiert; destruktives CheckBlock bleibt offen | teilweise | [source/hw/ps2_mc_files.c](../source/hw/ps2_mc_files.c); Exakte RPC-/Karten-Layouts, Persistenz, Schreibschutz und keine Datenkorruption |
| MC-03 | Raw-Page/Erase-RPCs fehlen | offen | [source/hw/ps2_mc_files.c](../source/hw/ps2_mc_files.c); Exakte RPC-/Karten-Layouts, Persistenz, Schreibschutz und keine Datenkorruption |
| MC-04 | Directory-Open/Read-Descriptors fehlen | offen | [source/hw/ps2_mc_files.c](../source/hw/ps2_mc_files.c); Exakte RPC-/Karten-Layouts, Persistenz, Schreibschutz und keine Datenkorruption |
| MC-05 | Zweiter Kartenport/Slot/Multitap fehlt | offen | [source/hw/ps2_mc_files.c](../source/hw/ps2_mc_files.c); Exakte RPC-/Karten-Layouts, Persistenz, Schreibschutz und keine Datenkorruption |
| MC-06 | MagicGate und öffentlicher F0-Buspfad fehlen | offen | [source/hw/ps2_mc_files.c](../source/hw/ps2_mc_files.c); Exakte RPC-/Karten-Layouts, Persistenz, Schreibschutz und keine Datenkorruption |
| MC-07 | Andere Kartengeometrien/importierte Sonderfälle | teilweise | [source/hw/ps2_mc_files.c](../source/hw/ps2_mc_files.c); Exakte RPC-/Karten-Layouts, Persistenz, Schreibschutz und keine Datenkorruption |
| MC-08 | In-flight SIO2-DMA wird im Checkpoint abgewiesen | teilweise | [source/hw/ps2_mc_files.c](../source/hw/ps2_mc_files.c); Exakte RPC-/Karten-Layouts, Persistenz, Schreibschutz und keine Datenkorruption |
| RPC-01 | LOADFILE-Erfolg teils synthetische Modul-ID | teilweise | [source/core/ee/ee_core.c](../source/core/ee/ee_core.c); Reale Modul-/Datei-/Transferzustände statt synthetischer Erfolgsmeldungen |
| RPC-02 | FILEIO/RSPU2/PADMAN-HLE statt allgemeiner IOP-Ausführung | teilweise | [source/core/ee/ee_core.c](../source/core/ee/ee_core.c); Reale Modul-/Datei-/Transferzustände statt synthetischer Erfolgsmeldungen |
| RPC-03 | SIF-DMA-IDs/Lebensdauer nicht vollständig modelliert | teilweise | [source/core/ee/ee_core.c](../source/core/ee/ee_core.c); Reale Modul-/Datei-/Transferzustände statt synthetischer Erfolgsmeldungen |
| RPC-04 | IOP-Modulgeräte-/Import-/Export-/Startzustand | teilweise | [source/core/ee/ee_core.c](../source/core/ee/ee_core.c); Reale Modul-/Datei-/Transferzustände statt synthetischer Erfolgsmeldungen |
| CDVD-01 | NCMD/SCMD/Tray/Disc-Typ-Matrix | teilweise | [source/hw/iop_cdvd.c](../source/hw/iop_cdvd.c); Command-/Fehler-/Sektorlayout-/Tray-/Timingmatrix und echte Daten |
| CDVD-02 | CDDA/RAW-CD/DVD-Sonderformate und Laufwerkszeit | teilweise | [source/hw/iop_cdvd.c](../source/hw/iop_cdvd.c); Command-/Fehler-/Sektorlayout-/Tray-/Timingmatrix und echte Daten |
| PAD-01 | Pad-Modi/Pressure/Vibration/Multitap | teilweise | [source/hw/iop_sio2.c](../source/hw/iop_sio2.c); Serieller/RPC-Pfad, Modi, Ein-/Ausgabe und Abwesenheit |
| DEV-01 | DEV9/HDD/Netzwerk fehlt als vollwertiges Gastgerät | optional_abwesend | [source/core/iop/iop_core.c](../source/core/iop/iop_core.c); Benötigte Gerätefunktionen von korrekt konfigurierter Abwesenheit trennen |
| DEV-02 | USB/i.Link-PS2-Geräte nicht allgemein emuliert | optional_abwesend | [source/core/iop/iop_core.c](../source/core/iop/iop_core.c); Benötigte Gerätefunktionen von korrekt konfigurierter Abwesenheit trennen |
| MEM-01 | Dynamischer Wii-MEM2-Peak nicht gemessen | hardware_nachweis | [source/core/checkpoint.c](../source/core/checkpoint.c); Pointerfreie Versionierung, Fehlerpfade und dynamische Wii-Gesamtbelegung |
| MEM-02 | Alle optionalen Gerätezustände checkpointfähig machen | teilweise | [source/core/checkpoint.c](../source/core/checkpoint.c); Pointerfreie Versionierung, Fehlerpfade und dynamische Wii-Gesamtbelegung |
| PERF-01 | Aktuelle Hotspots pro identischer Wii-Szene fehlen | hardware_nachweis | [source/core/runtime_profile.c](../source/core/runtime_profile.c); Gleicher Gastweg/Bild/Retirement/Optionen und echte Wii-Gesamtzeit |
| GAME-01 | SCPH-50004-BIOS-Kacheln ungeklärt | hardware_nachweis | [docs/CODEX_HANDOFF.md](../docs/CODEX_HANDOFF.md); Identischer Gastzustand, echte Wii-Bilder, steuerbare Szene und frischer Logbeleg |
| GAME-02 | Crash E3 ohne steuerbaren Gameplaybeleg | hardware_nachweis | [docs/CODEX_HANDOFF.md](../docs/CODEX_HANDOFF.md); Identischer Gastzustand, echte Wii-Bilder, steuerbare Szene und frischer Logbeleg |
| GAME-03 | Tekken ohne neuen steuerbaren 3D-Kampfbeleg | hardware_nachweis | [docs/CODEX_HANDOFF.md](../docs/CODEX_HANDOFF.md); Identischer Gastzustand, echte Wii-Bilder, steuerbare Szene und frischer Logbeleg |
| GAME-04 | GT3-Kartenabfrage/Rennstart unbelegt | hardware_nachweis | [docs/CODEX_HANDOFF.md](../docs/CODEX_HANDOFF.md); Identischer Gastzustand, echte Wii-Bilder, steuerbare Szene und frischer Logbeleg |
| GAME-05 | KOF-Zielszene/Fassung unvollständig untersucht | hardware_nachweis | [docs/CODEX_HANDOFF.md](../docs/CODEX_HANDOFF.md); Identischer Gastzustand, echte Wii-Bilder, steuerbare Szene und frischer Logbeleg |

## Arbeitsregeln

- Bei jedem Batch IDs, Codeänderung, tatsächlich ausgeführte Tests und Restgrenzen fortschreiben.
- Ein implementierter Teilpfad schließt die übrigen Teilziele seines Moduls nicht automatisch.
- Bewährte Interpreter-/Software-GS-Oracles, RAM-/Mapping-/IRQ-/Budget-Gates und EABI erhalten.
- BETA12.29: EE-Control und SPU2 Noise/PMON/Sweeps abgeschlossen; GETENTSPACE als MC-Teilziel.
- Als Nächstes: Gaussian/Reverb/ENVX, weitere MCSERV-RPCs und GS/GX-/JIT-Grenzen.
- Der unabhängige nächste Hardware-Schritt bleibt BIOS/Crash/Tekken im identischen Gastzustand.
- Keine Nutzerdaten/BIOS/Discs/Karten/Profile in Tests oder öffentlichen Berichten.
