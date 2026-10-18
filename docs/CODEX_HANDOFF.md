# Gekko2 → Codex: vollständiger technischer Handoff (15.10.2026)

**BETA12.31c network setup:** branch `codex/network-logger-menu`;
[report](BETA12.31c-NETWORK-SETUP.md). Wii IP/port settings, saved OFF-by-default,
live SDK status and explicit acknowledged test without BIOS. Receiver 1.1 adds
empty-capture diagnostics. No established network-failure cause or GX/BIOS/FPS fix.

**BETA12.31b optional network telemetry:** branch `codex/network-runtime-telemetry`.
[Report](BETA12.31b-NETWORK-PROFILE.md), [Windows setup](../tools/windows-telemetry/README-DE.md).
Default OFF, bounded queue/background UDP, fixed numeric scopes and options,
receiver bundles one ZIP. No persistent ChatGPT link, raw guest data or remote control.
No GX/BIOS/FPS fix. See current report for actual validation/platform limits.

**BETA12.31a CPU status correction:** branch `codex/cpu-status-log-review`;
[report](BETA12.31a-CPU-STATUS.md). Launcher uses active/requested CPU masks;
profile analyzer warns about interpreter and mixed configurations. No GX/BIOS
visual or FPS fix. Owner assets remain private.

**Aktuell: BETA12.31 Runtime Profile / Wii Limits 1.1**, Branch
`codex/runtime-profile-bios-investigation`, Buildcommit `ee6306d`, Draft-PR #40.
[Bericht](BETA12.31-RUNTIME-PROFILE.md): native EE-/VU-Kategorien, Scope-Aufrufe,
Arbeitsmengen, getrennter CPU-/Present-Auswerter und MEM1↔MEM2-Ausrichtungsfälle.
83 Host-Suites, alle 16 gelinkten PPC-Prüfgruppen und beide
Wii-DOL-/ELF-/MEM1-Gates bestanden. Keine neue physische Wii-/Dolphin-Ausführung,
kein FPS- oder BIOS-Kachel-Fix. Besitzer-BIOS lokal inventarisiert, partiell
disassembliert und bis regulärer Sprachauswahl ausgeführt; alle abgeleiteten
Daten strikt außerhalb von Git/Paketen. BIOS-Hardwaremodell nicht allein aus
ROMVER behaupten. Historische Absätze bleiben als Verlauf erhalten.

**Zusätzlich: Wii Limits 1.0**, eigener Branch `codex/wii-limits-benchmark`,
Buildcommit `da75aa5`, Draft-PR #39. [Standalone-Benchmark und geprüfte Downloads](WII-LIMITS-BENCHMARK.md).
87 Host- und 75 tatsächliche ELF-PPC-Prüfungen sowie DOL-/ELF-/MEM1-Prüfung
bestanden; Plattformdienste/GX-Fence für PPC-Prüfung simuliert. Keine neue
Wii-/Dolphin-Messung oder Emulatoränderung. CSVs unter `sd:/gekko2-bench/`.
Der separate Profiler-PR #38 wurde nur gelesen und nicht geändert.

**Aktuell 18.10.2026: BETA12.30**, `codex/beta12-30-graphics-transport-integration`,
Emulator-Buildcommit `640a761`, Integrations-PR #37, getrennte Code-PRs #32–#36.
[Bericht](BETA12.30-GS-VIF-ACCURACY.md), [Batchliste](COMPLETION-BATCHES.md),
[fortgeschriebener Katalog](COMPLETION-CATALOG.md). GS-CSR/IMR-Narrowzugriffe,
pending IMR, Q/K/L-LOD/MTBA/Mipfilter, VIF-CPU-Backpressure/FQC und writable ENVX.
78 Host-Suites, alle 14 gelinkten PPC-Prüfgruppen (einschließlich je 40 neuer
Accuracy-Fälle auf beiden ELFs) und beide Wii-DOL-/ELF-/MEM1-Gates bestanden.
Keine physischen Wii-/Dolphin- oder neuen Spiel-/BIOS-Kachel-/FPS-Nachweise.
Folgende BETA12.29-Absätze sind historisch; deren offene Ziele im Katalog fortführen.


**Aktuell 18.10.2026: BETA12.29**, Branch
`codex/beta12-29-completion-integration`, Emulator-Buildcommit `9fe55bf`,
Draft-PR [#31](https://github.com/Mafiacoding/Gekko2/pull/31).
[Bericht](BETA12.29-CORE-COMPLETION.md), [Katalog](COMPLETION-CATALOG.md)
und maschinenlesbare `COMPLETION-CATALOG.json` fortführen.
Drei Implementierungsbatches: EE-Control (C.F.S, einzelne native BC2, RI),
SPU2 Noise/PMON/Sweeps und kompatible Checkpoints, GETENTSPACE.
70 Host-Suites, 352 Emitter-PPC-Fälle und 98 gelinkte EE-Vergleiche bestanden;
beide devkitPPC-Builds samt DOL-/ELF-/statischem MEM1-Gate bestanden.
Auch alle 12 gelinkten PPC-Suites bestanden (105 Chain-, 24 ALU-, 72 RAM-/
TLB-/DMA-SMC-, 112 Sprung-/IRQ-Fälle, 8 Optionskombinationen und GS/GIF/VIF/IPU-
Gates). Vollständiger Prüfumfang im aktuellen Bericht, ältere Absätze historisch.

Nächste echte Codepunkte: SPU-04/05/06 (Gaussian/Reverb/ENVX und stille
Stimmen), MC-02/03/04 (CheckBlock/Raw-Page/Directory-Descriptors), JIT-03/04
(dirty Writeback und breiteres gemeinsames Mapping), GS/GX-Mipmap/Feedback/
EXTWRITE. Nur mit eigenständigen Oracles und bestehenden Sicherheitsgrenzen
fortführen. Audit umfasst nun source **und include**, 199 Dateien/274 kanonische
EE-Formen; Zulassung allein belegt keine komplette Semantik oder VU-Abdeckung.
Kein neuer Wii-/Dolphin-Lauf, keine neuen Spiel-/BIOS-Kachel-/FPS-Nachweise.


**BETA12.28 – Alpha-Core-Teststand:** [Menü, CLUT, Zweikreis-Scanout, PS2-Karten/FAT/MCSERV und IPU-Retries](BETA12.28-ALPHA-CORE.md). Keine neue Wii-Spielbarkeits-/FPS-/BIOS-Kachelbestätigung. Ältere Absätze beschreiben historische Versionen.

**Historischer Audit vor BETA12.28 (17.10.2026):** [Arbeitsplan mit 18 Paketen](ALPHA-ARBEITSPLAN.md),
[vollständiger Menüaudit](ALPHA-MENUE-AUDIT.md) und
[gepinntes PCSX2-/Play!-Review](ALPHA-REFERENZABGLEICH.md).
Basis BETA12.27 / `c51ab567`; Menü-/Modulumsetzung und Wii-Freigabe bleiben offen.
55 Host-Suites und zehn Zusatzprüfungen erneut bestanden; Optionsprobe zeigt
erzwungene GX-Testbits trotz All-OFF. Kein neuer Emulatorbuild/-Fix in diesem Audit.

**BETA12.27:** [gespeicherter GX-Risikomodus](BETA12.27-GX-RISK.md),
Standard OFF, wirksam erst bei Cold Boot. Geeignete Dreiecke dürfen einmal
komplett an GX statt in Zeilenabschnitten übergeben werden. Kantenpräzision
ist experimentell; Alpha-/Z-Logik, Fences und sichere Rückfälle bleiben.
Branch `codex/beta12-27-gx-risk-mode`, Basis `dea99dd`. Kein Wii-FPS-Beleg.


**BETA12.26:** [gemeinsamer GS→GX-Ausbau](BETA12.26-COMMON-GX.md):
Fog, HIGHLIGHT/HIGHLIGHT2, geeignete variable Vertex-Alpha-Kombinationen,
untexturierte Dreiecke, alle vier Clamp-Modi und gepolsterte kleine Texturen.
Keine Spielnamen-Auswahl; alle passenden GS-Zustände nutzen denselben Pfad.
Branch `codex/beta12-26-generic-gx`, Basis `b2c0a20`. Keine bestätigten Wii-FPS,
Gameplay- oder Kachelkorrekturen. Komplexes Feedback/Mipmaps bleiben offen.


**BETA12.25:** experimenteller GX-Dreieckspfad für große Texturen, variable
Z-Werte und GS-Alpha-/DATE-/AFAIL-Berechtigungen, nativer Texturcache und
Kontrollbuild. [Implementierung, Tests und offene Grenzen](BETA12.25-GX-TRIANGLES.md).
Branch `codex/beta12-25-tekken-gx`, Basis `eaccd98`. Tekken ist weiterhin nicht
vollständig GPU-basiert; keine bestätigte Wii-Spielbarkeits-/FPS-/Kachelkorrektur.


**BETA12.24a:** Automatische 4:3-Verkleinerung zurückgenommen. Die Nutzerkorrektur bezieht sich auf die Fragezeichen-Kacheln in Tekkens Charakterauswahl, nicht auf die gesamte Ausgabe. Wieder volle Ausgabegröße; BETA12.24-Diagnosen bleiben aktiv. Die Kacheln selbst sind noch nicht als korrigiert bestätigt. [Hinweis](BETA12.24a-FULLSCREEN.md).

**BETA12.24 – Spiele-Diagnose:** begrenzte GIF/XGKICK/SIGNAL-/VIF-/Interrupt-Ereignishistorie, getrennte GS-/INTC-Quittierungen, erste unbekannte VU-Paare und EE-Warteschleifen-Kontext. [Anleitung und Testbericht](BETA12.24-GAME-DIAGNOSTICS.md). Keine neue Gameplay- oder Stall-Fixbehauptung; BETA12.23 bleibt Rückfallversion.

Branch `codex/beta12-24-game-diagnostics`, Basis `bdd8230`.

**BETA12.23 – Crash/Tekken-Logbatch:** VU-Integer-Lanemasken, VIF-Wartebedingungen und GS-UV-Nachkommabits korrigiert. Standard-4:3-Ausgabe auf einer als 16:9 konfigurierten Wii wird mit Seitenbalken dargestellt. [Analyse, Referenzabgleich und Testbericht](BETA12.23-GAME-LOG-REVIEW.md). Crash zeigt im BETA12.22-Log einen dauerhaften SIGNAL-/XGKICK-Stall; dessen vollständige Ursache und Gameplayfortschritt bleiben auf neuer Wii-Hardwareausführung zu prüfen. Kein belegter Gesamt-FPS-Gewinn.

Branch `codex/beta12-23-game-log-fixes`, Basis `8553e2d`. Nachstehende Checkpoints sind historisch.

**BETA12.22 – GS/GX-Formate und Fehlerbehandlung:** alle 13 gültigen GS-Texturformate werden für GX nach RGBA8 dekodiert. CT32/24/16/16S sind für GX-Scanout verfügbar; CT16/16S verwenden bei geeigneten Zeichenoperationen einen GX-Snapshot mit präzisem GS-Readback. Dazu kommen Korrekturen für CSM2-Paletten, AFAIL-Alpha, XGKICK und IPU sowie eine Pause bei terminalem GX-Fehler. [Build- und Testbericht](BETA12.22-GS-GX-FORMATS.md). Chain-Residenz und experimentelle lokale PSM-Konvertierung bleiben standardmäßig OFF. Crash-Schwarzbild, lila Ausgabe und BIOS-Kacheln sind auf echter Wii weiterhin nicht als behoben bestätigt.

Branch: `codex/gx-xgkick-ipu-recovery`, Basis `c2c5bc471e712e87ca211c532ce8abd445e0240a`. Die bisherigen Abschnitte bleiben als Verlauf erhalten.

**Neuer experimenteller Arbeitsstand:** [BETA12.21](BETA12.21-EXPERIMENTAL-OPTIONS.md),
Branch `codex/beta12-21-experimental-options`, auf IPU- und Integrationsbatches.
Geprüfte 128-Bit-Native-Ketten einschließlich ausgewählter RAM-/Sprungkörper
existieren jetzt opt-in; universeller Native-ABI/Dirty-Writeback bleiben offen.
Die folgende main-/BETA12.20-Beschreibung ist der historische Ausgangspunkt.
Keine bestätigte Crash-/BIOS-/Gameplaykorrektur auf echter Wii.

> **START HIER.** Dieses Dokument bündelt die tatsächlich bekannte Projektgeschichte und die nächsten Entwicklungsarbeiten. Es ist ein **Engineering-Handoff**, kein Beweis vollständig implementierter PS2-Hardware. Zusätzlich `AGENTS.md`, `STATUS.md`, `README.md` und die einzelnen `docs/BETA12.*`-Berichte lesen. Den aktuell ausgecheckten Commit/Release-String immer selbst prüfen.

## 1. Projekt / Ziel / Stand
- Name: **Gekko2**, experimenteller PlayStation-2-Emulator für **Nintendo Wii** (Homebrew Channel; Dolphin zum Testen); Ursprungsidee PCSX2-Wii, umfangreiche Eigenentwicklung. Besitzer arbeitet mit Assistenz an dem Projekt; Benutzerkommunikation auf Deutsch.
- GitHub: https://github.com/Mafiacoding/Gekko2; GitHub-Pages: https://mafiacoding.github.io/Gekko2/; Projektunterstützung: https://ko-fi.com/teamgekko2. Nicht ohne Auftrag an Homepage, Branding oder Spendenfunktionen arbeiten.
- Der Code-Stand **BETA12.20** wurde am 15.10.2026 auf `main` übernommen (Basis-Commit `5d3b3d3e9f91b850e3a996d1a6892b194b855ae2`). Die nachfolgende Agenten-Dokumentation erzeugt weitere Doku-Commits; die relevante **Code-Version** bleibt `include/build_version.h`: `R1389-SPEED-BETA12.20-GX-ALPHA-FENCES`. Kein nachweislicher Wii-Spieltest dieses Builds.
- Zentraler Nutzerwunsch: **Crash E3 aus dem Black Screen bis zum Gameplay bringen**, Tekken Tag in den 3D-Kampf, GT3 über Copyright/Memory Card hinaus, BIOS-Kacheln vollständig und korrekt darstellen; möglichst schneller ohne falsches PS2-Timing.
- Kein vollständiger Dynarec, kein vollständiger GX-GS, kein vollständiges MPEG/IPU oder perfekter VU-Takt. Bei Code/Ausgabe nicht „fertig“ behaupten, nur weil ein Host-Test besteht.

## 2. Architektur / Einstiegspunkte
| Komponente | Hauptdateien | Anmerkungen |
|---|---|---|
| EE-Interpreter, CPU-Status, Scheduling | `source/core/ee/ee_core.c`, `source/core/recompiler/ee_jit.c` | EE 128-bit GPR; EPC/BD, Exception- und IRQ-Präzision kritisch |
| PPC EE-Dynarec / Blockcache | `source/core/recompiler/ppc_dynarec.c`, `ppc_residency_internal.h`, `ppc_code_cache.c`, `ppc_ee_mmi.inc` | Codegenerierung, Register-Lanes, generationssichere Verknüpfungen |
| IOP / Lightrec | `source/core/iop/iop_core.c`, `source/core/recompiler/iop_jit.c`, `iop_lightrec.c` | Idle-Skip, IOP-IRQ/Thread, MIPS IOP-Exceptions |
| VU0/1 | `source/hw/vu.c`, `source/core/recompiler/vu_jit.c`, `source/hw/vu_fmac_internal.h` | FMAC/Q/P/EFU, Pipeline, XGKICK |
| GS/GIF/VRAM | `source/hw/gif.c`, `gs_mem.c`, `gs_gx.c`, `gs_gx_surface.c`, `gs_gx_additive.c` | 4 MiB physisches VRAM, GS-PSM-Swizzle, GX/CPU-Fallback |
| Wii-Bildausgabe | `source/hw/gs_wii_output.c`, `source/main.c` | Scanout PSMCT16/16S/24/32, Present/FB-Rollover |
| VIF1/DMA/SIF/CDVD | `source/hw/vif.c`, `dma.c`, `sif.c`, `iop_cdvd.c`, `iop_cdrom_legacy.c` | Trennschärfe GS FIFO / DMA / IRQ / FILEIO |
| IPU/MPEG | `source/hw/ipu.c`, `ipu_mpeg.c`, `ipu_vlc.c`, `ipu_idct.c`, `ipu_pack.c`, `ipu_csc.c` | Nur Teilimplementierung |
| Launcher, Menüs, SD, Logs | `source/main.c`, `source/hw/frontend_*.c`, `source/hw/frontend_log.c` | GX-/CPU-Optionen, Datei-Selektor, Cold Boot Logs |
| Build/Checks | `Makefile`, `tools/build_beta12_19_fb16_jit.sh`, `tools/verify_dol_r1352.py`, `tools/verify_wii_memory.py` | Modernes devkitPPC/libogc; Testflags beibehalten |

Die vorhandenen `docs/reference/pcsx2/`-Referenzquellen und `docs/R1384-CODEX-HARDWARE.md` erklären ältere Implementierungsentscheidungen; sie ersetzen nicht die aktuelle Codeinspektion. Lizenzbedingungen von PCSX2, Play! und sonstigen Projekten beachten.

## 3. Funktionierende / implementierte Teilpfade (nicht überinterpretieren)
- PS2-BIOS bis OSDSYS/Browser/System Configuration wurde mit älteren Builds tatsächlich erreicht; Sony/Namco-Bilder und Tekken-Charakterauswahl teils ebenfalls. Wii-Vollgeschwindigkeit **nicht** erreicht.
- EE/IOP-PPC-Dynarec, AOT/Profile, Code-Cache, teils direkte Kanten, Event-/Idle-Skip-Scheduler, GX-Renderer und fallback Software-GS sind real vorhanden.
- VU-FMAC-Timing, Q/P, XGKICK/GIF arbitration, SIF, DMA, VIF und GX-Pfade wurden über viele R13xx/BETA-Schritte erweitert. Ihre Vollständigkeit **nicht voraussetzen**.
- CHD, BIN/ISO (abhängig von Image/Frontend), Legacy Sony FILEIO für die Crash-Demo, Host-GS-AOT und GX-Diagnosen wurden ergänzt.
- `TRXDIR=2` Local→Local mit Formatlayouts/Overlap-Snapshot, `TRXDIR=1` Local→Host FIFO und **normalem** GS→VIF1→EE-RAM-Readback (ohne sämtliche Reverse-Chain-Fälle), VRAM-Alias-Seitenprüfung, 4-MiB-Wraparound, GS GPU/CPU Readback-Fences.
- PSMCT16/16S/24/32 Scanout und 16-Bit-Ziel-FB-Schreibzugriffe verbessert. GX CPU-Import mit DATE, ATE, ATST, AFAIL, FBA, FBMSK und Z-Test für ausgewählte texturierte Sprites; komplexe 3D/TEV/Framebuffer-Feedback-Fälle fallen **weiter auf Software** zurück.
- 128-Bit-GPR-Lanes innerhalb nativer Blöcke in PPC `r18..r29`; **keine persistente Übergabe über unabhängige Blocks**.
- Experimentelle Konvertierungen zwischen nicht hardware-bestätigten Pixelformat-Tiefen existieren als getrennte, **standardmäßig abgeschaltete** Option. Nicht automatisch für alle Spiele aktivieren.

### BETA12-Änderungsfolge (für Git-Bisect / Rückfallpunkte)
| Version | Wesentlich | Erprobung / Grenze |
|---|---|---|
| 12.1–12.5 | Crash-E3 Boot/File-IO, Diagnose, Legacy FILEIO `0x30313132` | Font-OPEN-Offsets und Pfade verfolgt |
| **12.6** | FILEIO-OPEN-Path-Offset 8 statt 0/4 | `FONTS/ARIEL32.FNT` konnte gelesen werden; Spiel noch nicht sichtbar im Gameplay |
| **12.7/12.8** | VBlank-Poll-Fast-Wait, verallgemeinert | Crash nahe 2 *emulierten* FPS; ein Teil des Gewinns kann nur VBlank-Poll-Überspringen sein |
| **12.9/12.10** | GS Local→Local `TRXDIR=2`, PSM-Layouts/Overlap/GX-Fences | Host-Tests; keine nachgewiesene Blackscreen-Lösung |
| **12.11** | PSM-Layouttransfer; experimentelle Cross-Depth-Konvertierung schaltbar | Experimentelles Verhalten nicht als native GS-Semantik behandeln |
| **12.12** | GS→Host FIFO, BUSDIR/VIF1 Reverse-Normal-DMA | Reverse Chain / Timing-Sonderfälle offen |
| **12.13** | GS VRAM-Alias/GX-Readback, Snapshot bei Cross-Layout-Overlap | Komplexe Layout-/VRAM-Fälle offen |
| **12.14** | 4-MB Wrap, VU1 XGKICK Pending, IOP-JIT-Grenzen, IPU Idle, DMA CALL/RET | Fünf Teilbatches, **nicht** fünf komplette Subsysteme |
| **12.15** | EE-JIT-Lookup, IOP-Tick, GX-Cache-Skip | Keine gesicherte reale FPS-Steigerung |
| **12.16/12.17** | EE-Exceptions, weitere MMI, IOP/JIT-Kanten, GX Z16/Z24, Register-Lanes | Alle Helferfälle und Global-ABI weiter offen |
| **12.18/12.19** | PSM16 Scanout, Exception-Prolog-Guard, FB16/FBA/FBMSK/DATE | Crash-E3-Schwarzbild nutzt bisher 32-Bit-FB; kein Crash-Fix behaupten |
| **12.20** | GX-Sprite-Date/ATE/AFAIL/FBMSK CPU-Pixel-Fences + JIT-ABI-Audit | Host-Tests: 159 neue Alpha-Prüfungen, 324 alte Pipeline-Fälle, 16 FB16-Fälle; echte Wii offen |

Berichte in `docs/BETA12.*`; mehr historische Details in `docs/R1384-CODEX-HARDWARE.md`, `docs/R1389-Claude-Handoff.txt`, `docs/R1389-GX-Alpha.md`.

## 4. Spielreproduktion / bekannte Blocker

### Crash Bandicoot: Wrath of Cortex E3 Demo (legales vom Nutzer bereitgestelltes privates Image)
- Der Nutzer hat `crash.bin` privat bereitgestellt; es darf **niemals** nach GitHub oder in öffentliche Testreports kopiert werden. Codex in Cloud-Umgebungen hat keinen Zugriff auf diese Datei, sofern der Nutzer sie nicht ausdrücklich separat bereitstellt.
- Initial `NuError nufont.c(117): unable to create font fonts\\ariel32`. Auf der CD sind u. a. `FONTS/ARIEL32.FNT;1`, `ARIEL32.BMP;1`. BETA12.6 behebt die FILEIO-OPEN-Dekodierung. Frühere Logs danach: GX Draws, VSync-Polling um EE PC `0x001FC970..0x001FC988`, aber vorderer Framebuffer schwarz (zu diesem Zeitpunkt **PSM=0 / 32-bit**).
- Die Demo fragt unter anderem nach `LEVELS/B/TITLES/TITLE.LGT`; diese Datei war auf dem analysierten Image nicht vorhanden. Es existieren `TITLE.GSC` und `TITLE.NUS` sowie Dateien unter `ATS/LEVELS`. **Nicht erwiesen**, dass das fehlende LGT den Blackscreen verursacht; keine Dateierfindung oder Blind-Stubbing.
- Nächste Instrumentierung: primäre GS-DISPLAY/PMODE-/DISPFB-Register, Framebuffer-BP/BW/PSM/Offset, tatsächliche EFB→XFB Presents, Front-/Backbuffer-Hashes, GIF PATH1/3, VU1-Geometry, GX Draw akzeptiert/verworfen, CDVD/FILEIO-Returncodes. Erst festlegen, ob Bild nie gerendert, ins falsche Ziel geschrieben, nicht präsentiert oder vom Spiel bewusst schwarz gesetzt wird.
- Gezielt `GX` vs `Software GS` vergleichen; gleicher EE-PC/IRQ/framecount/GS-Dump und bei Bedarf PCSX2-GS-Pakettrace. Emulierte FPS allein kein Erfolgskriterium.

### Tekken Tag Tournament
- Frühere Host-Wegpunkte: Namco, Auswahl bis VS/3D-Fight, Geometrie noch falsch; auf echter Wii nach Charakterwahl schwarzer Bildschirm; im Lauf der Entwicklung teils ~0,30–0,31 emulierte FPS gemeldet, **nicht** bestätigt für BETA12.20.
- Prüfpfad: GIF/VIF DMA-Akzeptanz, VU1-Start + XGKICK PATH1 vs PATH2/3 Arbitration, TLB/GS Target Sync, input/coin-event; mit präzisen IRQ-/EE/IOP-Counters vergleichen.

### Gran Turismo 3
- Copyright-Bildschirm erreicht; danach wiederholte Memory-Card-Fragen und Hänger; noch keine Rennen. SIF-RPC/CDVD/Memory-Card-Protokoll, IOP-Threads und Polls untersuchen, nicht blind Memory Card „OK“ zurückgeben.

### BIOS / King of Fighters
- BIOS-OSDSYS erreicht, aber Kacheln/Framebuffer-Updates teilweise falsch oder dunkel, lange Ladezeiten. KOF bleibt unvollständig untersucht. Testreihenfolge BIOS/OSDSYS + Crash E3 + Tekken + GT3; Regressionen getrennt.

## 5. Hauptaufträge für Codex – fünf priorisierte Batches

### Batch A – Crash Blackscreen: Beweis führen statt kaschieren
1. **Zuerst** Instrumentierung: separate Ring-/begrenzte Logs für EE-PC, GS DISPFB/PMODE/DISPLAY, effektive FB-Adresse und PSM, Draw/Present, GPU→CPU Sync, VRAM-Hash, GIF-PATH, VU1-XGKICK, FILEIO-Fehler. Logging muss drosselbar sein (Wii-FPS); keine endlosen spam-/flackernden Logs.
2. Isolierbare Fehler anhand **synthetischer** Regressionstests beheben; Software-GS als Referenz. Kein Backbuffer-Tausch und kein künstliches Rendering.
3. Testplan und konkret erforderliche Wii-Logzeilen liefern. Kennung eindeutig.

### Batch B – echte persistente 128-Bit-JIT-Register über unabhängige Blöcke
1. Aktueller Compiler: `source/core/recompiler/ppc_dynarec.c`, `ppc_residency_internal.h`. `ee_precise_block_emit` sichert die genutzten `r18..r29` per PPC ABI pro Block; `ppc_dynarec_translate_ee_cached_chain()` ruft unabhängige `ee_precise_fn` über `bctrl` auf. Bisher wird die EE-Repräsentation write-through aktualisiert.
2. Baue **zusätzlichen**, optionalen Native-to-Native-Fast-ABI statt aktuelle C-ABI zu brechen; gemeinsames Gruppen-Lane-Mapping inklusive 128-bit HI/LO/SA, definierte Eintritts- und Austrittsregeln.
3. Guards an **jeder** Kante: Guest-PC, EE-State-Identity, Code-Cache-Generation/Eviction, RAM-Inval inklusive DMA/SMC, TLB/ASID/Mapping, Interrupt-/Scheduler-Budget, Exceptions/Branch-Delay/annulled branches.
4. Jede nicht validierte Kante / Helper / IRQ / Exception / Interpreter-Rückkehr **flusht dirty Lanes** bevor ein normaler C-ABI-Call oder Callback erfolgt. Niemals Host-Register-Saves einfach entfernen. Zuerst opt-in, Default sicher.
5. Differential-oracle: zufällige und gerichtete 128-Bit-GPR/HI/LO/SA-Tests; Hilfsaufrufe, Branch-Delay-Exceptions, invalidierte Codepages, self-modifying DMA, Asynchronität und Block-Eviction. Erst dann Default erwägen.

### Batch C – GS/GX Vollständigkeit
- Bis BETA12.20 DATE/ATE/AFAIL plus FBA/FBMSK **nur** für begrenzten GX-Sprite-Snapshot/CPU-Import; residenter EFB-TEV/Blend/Depth, texturierte Dreiecke, 16-Bit-Import, AA1, COLCLAMP/PABE/FIX, Dithering, Texture-Shuffle, komplexes GS-Aliasing/Readback und Feedback-Loops sind **nicht** als vollständig korrekt anzunehmen.
- Ergänze exakte GS-Pixel-Oracle-Tests für alle GS ALPHA(A,B,C,D,FIX)/TEST/DATE/DATM/ATE/ATST/AFAIL, Z-Formate, Ziel-PSMs 16/16S/24/32, SCANMSK und FBMSK. CPU/GX-Bildparität zuerst; bei unbewiesenen Hardwarefällen **Software-Fallback** behalten.
- Diagnosezähler GX-Accept/Reject nach Ablehnungsgrund, tatsächliche EFB-Ausgabe und Cache-/Fence-Aktivität; visuelle Wii-/Dolphin-Vergleiche erforderlich.

### Batch D – VU/GIF/Scheduler/EE-IOP-Timing
- FMAC-MAC/STATUS/CLIP, Q/P Verzögerungen, EFU, VU0-M Synchronisierung; XGKICK/PATH1/2/3 Arbiter, VIF direkt, FIFO-Full/Pause/Replay, GIF-SIGNAL Double-Stall, DMA pausiert/fortgesetzt.
- Event-driven EE/IOP Scheduler mit exakten IRQ-/SIF-/DMA-/GS-Fences statt winzigen Lockstep-Zeitschritten. Vergleiche *zeitliche Semantik* (nicht nur schnelle IRQ-Zahlen) mit PCSX2 und vorhandenen Tests. Frühere Hotspots: EE 8 Slots pro IOP-Tick, Scheduler 22–28 %, IOP Idle, viele EE-Blocklookups — **historische Messung, vor Änderungen erneut profilen**.
- Nur eventbasierte Idle-Skips, die keine sichtbaren DMA-/IRQ-/Memory-Side-Effects übergehen.

### Batch E – weitere PS2-Hardware / Qualität
- IPU: BDEC/IDEC MPEG-2, VLC/IDCT/CSC/PACK, DMA-Backpressure und Timing, unabhängige Decoder-Oracles.
- GS `TRXDIR=1`: VIF1 Reverse-Chain und FIFO-Edge-Cases; `TRXDIR=2`: GS-Aliases/PSM-PSM-Transfers über VRAM-4-MB-Ring.
- CDVD/SIF RPC/Legacy FILEIO, GT3 Memory Card, Controller/Pad, SPU2 und CHD/ISO/SMB-Verhalten korrekt spezifizieren und gezielt testen.
- Jede Änderung in kleinen, reproduzierbaren Arbeitsschritten mit Host-Tests, Wii-Build, ggf. Dolphin, danach Hardware-Log. Keine „100 %“ Behauptung ohne Abdeckung.

## 6. Build-/Toolchain-Referenz
Empfohlen: **devkitPPC r50 / GCC 16.1.0, libogc 3.1.0** (historisch kompatibles DevkitPro-Setup), möglichst offizielles devkitPro. Ziel original Wii / PowerPC 750; Firmware-/ARM-/IOS-/DSI-Pfade optional, keine Annahme dass IOS222/ARM Worker auf jeder Wii aktiv ist.

```bash
export DEVKITPRO=/path/to/devkitpro
export DEVKITPPC="$DEVKITPRO/devkitPPC"
export PATH="$DEVKITPPC/bin:$DEVKITPRO/tools/bin:$PATH"
# Read and adapt the baseline to a NEW BUILD/TARGET; don't reuse old object dirs blindly.
bash tools/build_beta12_19_fb16_jit.sh
```

Für neue Versionen eigene BUILD-/TARGET-Namen verwenden, ebenso `include/build_version.h`. Die bestehenden Flags umfassen `GEKKO2_SPEED_RETIRE`, `SPEED_STUB`, `GX_NATIVE_BIOS_TRI`, `EE_AOT`, `VU_AOT`, `IOP_AOT`, `EE_BLOCK_FAST`, `GOURAUD_EARLY_DEGENERATE`, `ARM_BETA`, `DSI_FASTMEM`, `GX_NATIVE_ADDITIVE`, `GX_RESIDENT_FENCE`, `IDLE_SKIP` und `LIGHTREC_PILOT=1`; `-O1` ist aktuelles diagnostisches Build-Default im `Makefile`. Ein O1-Build ist **kein** verifizierter Speedvergleich mit anderen Optimierungsflags.

```bash
python3 tools/verify_dol_r1352.py <target>.dol <target>.elf
python3 tools/verify_wii_memory.py <target>.elf
```

- Beim Wechsel von Version/Flags **frisches Build-Verzeichnis** verwenden; veraltete `.o/.d` enthielten in früheren Builds alte Release-Strings.
- SD-Paket: `apps/gekko2/boot.dol`, `meta.xml`, `icon.png`; Paket-DOL muss bytegleich mit separat gelieferten DOL sein.
- `make` generiert Wii `.dol/.elf`; GitHub-Repo ist **Source**, niemals private BIOS- oder Spielekopie. GitHub CI kann einen Headless-Host-Regressionstest und Wii-Cross-Build anbieten, wenn Toolchain/Deps reproduzierbar installiert sind.
- Aktuelle Quellen unter `third_party/` enthalten bestehende libchdr, lightrec, lightning und PCSX2-Referenzteile. Lizenzdateien und Upstream-Notices nicht löschen. `data/pcsx2/bios/README.txt` nur Hinweis, **kein BIOS** im Repo.

## 7. Baseline-Tests (bevor/nachdem Änderungen erfolgen)
- `tests/test_beta12_20_gx_alpha.c` — 159 GS ATE/DATE/AFAIL/FBA/FBMSK-Prüfungen (Host).
- `tests/test_gx_pipeline_r1298.c` — 324 GX-vs-Software-GS-Pipeline-Kombinationen.
- `tests/test_beta12_19_framebuffer16.c` — 16 FB16-Schreib-/DATE-/Maskierungs-Prüfungen.
- `tests/test_beta12_17_depth.c`, `tests/test_beta12_17_framebuffer.c`, `tests/test_beta12_17_residency.c`, `tests/test_beta12_16_codegen.c` — GPU-Depth, MMIs, Lane-Gruppen.
- `tests/test_ee_exceptions.c`, `tests/test_jit_generation_r1316.c`, `tests/test_jit_cache_eviction_r1318.c`, `tests/test_iop_block_boundaries_beta12_14.c`, `tests/test_dma_call_ret_beta12_14.c`, `tests/test_ipu_idle_beta12_14.c`.
- `tests/test_gs_local_host_beta12_12.c`, `tests/test_dma_vif1_gs_readback_beta12_12.c`, `tests/test_gs_alias_sync_beta12_13.c`, `tests/test_gs_wrap_beta12_14.c`, `tests/test_gs_psm_convert_beta12_11.c`.
- `tests/test_xgkick_native_stubs_beta12_14.c`, `tests/test_beta12_18_scanout.c`, `tests/test_fileio_open_packet_beta12_6.c`.
- Manche Host-Tests inkludieren `.c` direkt und müssen mit passenden `-Iinclude -Isource -I.`, `-lm`, `-ffunction-sections/-Wl,--gc-sections` gebaut werden. Nicht blind `cc tests/*.c` linken: doppelte statische Testimplementierungen / optionale CHD-Codec-Abhängigkeiten. Fehlende Testdriver-Libchdr-Linkerflags sind kein Beweis eines Wii-Build-Problems.
- Dokumentierte frühere Checks nicht ohne erneutes Ausführen als „jetzt bestanden“ kennzeichnen.

## 8. Logs, Hardware-Evidenz und Leistung
- Bei echter Wii startet `source/hw/frontend_log.c` eine Zeile `SESSION checkpoint=<GEKKO2_RELEASE> ...`. Weitere Diagnosezähler in `source/main.c`:
  - `GS_LOCAL_HOST` (GS FIFO/Local-to-Host), `GS_ALIAS_SYNC` (GPU→CPU-Sync/Alias), `GS_LOCAL_COPY` (Local-to-Local),
  - `GS_SCANOUT_FORMAT` (Frontbuffer PSM), `EE_VBLANK_POLL` (sichere Poll-Skips),
  - `GX_TEXCACHE_INVALIDATE_SKIPS` (GX Cache-Skips).
- Ausreichend häufige zeitgestempelte Schnappschüsse statt logspamming; bei Black Screen letzten PC/IRQ/DMA-Zustand mit Render- und Present-Zählern korrelieren.
- Historisch wurden in frühen Builds EE-Durchsatz ~0,6 M instr/s / um 0,2 VBlank/s, BIOS 0,09–0,19 emulierte FPS, Tekken ~0,3 und Crash nahe 2 erreicht. Diese Zahlen stammen aus **unterschiedlichen Builds und Messmethoden**, teils VBlank-Skip-Metriken. Nicht direkt als BETA12.20-Ergebnisse oder echte Spielflüssigkeit präsentieren.
- Jeder PR-Bericht muss angeben: Commit SHA, Testkommando, tatsächlich beobachtete Ausgabe, DEVKITPPC-/libogc-Version, verifizierter DOL-/ELF-Status, relevante MEM1-Zahl, reale Wii- oder Dolphin-Ausführung (ja/nein), bekannte Regressionen, Vergleichsbasis.

## 9. Sicherheits- und Veröffentlichungsgrenzen
- **Streng privat:** benutzereigene `crash.bin`, weitere PS2 ISO/BIN/CHD, SCPH-BIOS, proprietäre AOT2-Profile/VU-Firmware-Dumps, laufende Wii-/Dolphin-Logs mit privaten Pfaden, Memory Cards. Öffentlich nur eigene Logs-Ausschnitte ohne personenbezogene Daten mit explizitem Nutzerauftrag.
- Keine synthetischen „fehlenden“ Spieldateien oder Cheat-Bypasses als Emulator-Korrektheit ausgeben. Prüfe echte GS/IOP-Semantik.
- Unverifizierte Hardwareoptimierung über kontrollierte opt-in-Schalter und Software-Fallback. Keine riskante Änderung an `main` ohne Test und Review; bei großen Batches eigener Branch/PR.
- PCSX2 und Play! als **Referenzen**, nicht als frei kopierbare Codebibliothek behandeln; Lizenzen beachten.

## 10. Sofortiger Arbeitsauftrag an Codex
1. Repo `Mafiacoding/Gekko2` öffnen und `AGENTS.md` lesen, diesen Handoff lesen, `include/build_version.h`, `STATUS.md` und `docs/BETA12.20-GX-ALPHA-RESIDENCY-PLAN.md` prüfen.
2. Von aktuellem `main` einen **Arbeitsbranch** erstellen; ersten kleinen reproduzierbaren Batch zur **Crash-E3-Blackscreen-Diagnose** oder zum **JIT-ABI-Guarding** auswählen und implementieren. Bevorzugt zuerst gezielte Crash-Draw/Present-Zähler, weil ohne Diagnose eine Fixbehauptung nicht belegbar wäre. Falls kein privates Disc-Image verfügbar ist, einen synthetischen GS/GIF-Testrahmen erstellen und genau benennen, welche Hardware-Logs der Besitzer liefern muss.
3. Für jeden Patch Host-Regressionen und nach Möglichkeit Wii-Cross-Build ausführen, Build-/FPS-Grenzen ehrlich berichten; Version ändern nur bei neuem releasefähigen DOL. Einen reviewbaren PR anlegen (nicht blind auf `main` pushen), mit Tests, Diff, Restproblemen.
4. Danach die Batches B (128-Bit native-to-native Fast ABI), C (GX/GS Accuracy), D (VU/GIF/Scheduler), E (IPU/CDVD/Peripherie) nacheinander in isolierten PRs abarbeiten. Umfangreiche Aufgaben ausdrücklich als mehrere verifizierbare Schritte behandeln.
5. **Niemals** „vollständig implementiert“, „Black Screen behoben“ oder „spielbar“ melden, bevor entsprechende Emulator-Lauf- und Bild-/Log-Belege vorliegen.

**Keine Hintergrundarbeit behaupten**: Codex muss in einer gestarteten Task arbeiten; das Vorhandensein dieser Datei startet noch keinen Lauf.
