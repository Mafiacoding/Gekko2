# Gekko2 Hybrid — Bestandsaufnahme und Architekturplan (nur Analyse, kein Code geändert)

Stand: 2026-11-10 · Basis: `integration/crash-e3-plus-a15` (`bde1228`, A19) · Autor: Claude
Dieses Dokument ist Phase 1 des Hybrid-Auftrags. **Es wurde kein Quellcode verändert.** Es stützt sich auf
(a) den A19-Quellcode, (b) die hochgeladene `Gekko2.elf` (nur Binäranalyse), (c) den Snapshot
`downloads/crash-gx-handoff-2026-10-29` (`Crash-GX-Complete-Source-Handoff.zip`: Verzeichnisse `PS2Recomp/`,
`Gekko2/`, `handoff/`) und (d) Messdokumente im Repo. **Nicht gemessen:** FPS, Zeitanteile auf der Wii. Alle
Zahlen zu 2 FPS / 0,20 FPS sind Angaben des Besitzers.

## 0. Wichtigster Befund vorweg: die hochgeladene ELF ist nicht A19

| Prüfung | Ergebnis |
|---|---|
| SHA-256 der ELF | `38c8f28ea0a2d29ea20fca26e5f2063cb1e644a9994feb949bb884f720e7bf54` (6.830.724 B) |
| Toolchain | devkitPPC GCC 16.2.0 (+ ein 15.2.0-Eintrag in `.comment`) |
| Versionsstring | `R1389-SPEED-BETA12.20-GX-ALPHA-FENCES` (= Stand von `main`, `96030ac`) |
| Symbolvergleich | 3764 Symbole. Ein Standard-`make` von `main` liefert 3763 (einziger Unterschied: `vu_exec_lower`). Ein Build von `integration`/A19 hat 4004 Symbole (Crash-Linie: +271). |
| Crash-Fix-Symbol `gif_signal_stall_count` | **fehlt** |
| A15-Symbol `ee_core_gpr_generation_bump`, Bridge `ee_recomp_bridge_bind` | **fehlen** |
| AOT-Einstieg `ee_aot_try_execute_fetched`, `ee_aot_block_count` | **fehlen** (keine AOT-Backend-Kompilation) |
| Größe text/data/bss | 1.226.292 / 413.398 / 20.187.808 (Standard-Build); der Build mit den Projekt-Flags aus `tools/build_r1389.sh` hat text 1.574.252 |

Folgerungen: (1) Die ELF ist ein **Standard-Build des BETA12.20-Stands** ohne die Crash-Korrekturen aus `e7aae58`
und ohne A11–A19. (2) Sie enthält **keine** PS2Recomp-erzeugten oder AOT-übersetzten Blöcke. (3) Sie wurde
**ohne** die Geschwindigkeits-Flags gebaut (`-DGEKKO2_EE_AOT -DGEKKO2_EE_BLOCK_FAST -DGEKKO2_DSI_FASTMEM …`).
Damit läuft sie mit dem Standardprofil, in dem der EE-Blockpfad **aus** ist
(`include/core/recompiler/optimization.h:14-17`: `GEKKO2_OPT_BLOCK_DEFAULT` ist 0 ohne `GEKKO2_EE_BLOCK_FAST`;
`GEKKO2_OPT_BASE_DEFAULT` entfernt `EE_BLOCKS` ausdrücklich).
Wenn Crash mit dieser ELF „ohne Grafikfehler bis ins Spiel" lief, steht das im Widerspruch zu
`docs/CRASH-E3-VU-TIMING.md` (BETA12.20 blieb dort am GIF-SIGNAL/VU-Stau hängen). **Bitte den Hash der DOL/ELF
bestätigen, mit der die 0,20 FPS gemessen wurden.** Ohne diese Klärung ist jede Aussage über „A19 = 0,20 FPS" unsicher.

## 1. Die zwei Codebasen (getrennt)

### A. Gekko2 A19 (diese Linie)
Ein vollständiger Emulator: echter BIOS-Boot, EE-Kern (`source/core/ee/ee_core.c`, 11.959 Zeilen), IOP-Kern
(`source/core/iop/iop_core.c`), VU0/VU1 mit getakteter Pipeline (`source/hw/vu.c`, `vu_fmac_internal.h`), GIF/GS
(`source/hw/gif.c` 3.612 Z., Software-Rasterizer + GX-Hybrid `gs_gx.c`), DMA/VIF, Scheduler mit **8:1-EE:IOP-Interleave**,
Block-JIT `ppc_dynarec.c` (7.696 Z.) + `ee_jit.c`, AOT-Backend `ee_aot.c` (nur Einstieg; Tabelle standardmäßig leer),
IOP-Lightrec-Pilot, optionale ARM/IOS-Beschleunigung. Die Zeitbasis ist **instruktionsgenau**.
A11–A19 (Parser, `tools/ee_aot_generate.py`) übersetzen geprüfte Upstream-Funktionen in dieses AOT-Format; das ist
Werkzeug und Test, **kein** Laufzeitbestandteil, solange kein privates Include gebaut wird.

### B. PS2Recomp 29.10 (Snapshot im Download-Branch, „sehr dreckiger" Arbeitsstand)
Ein **eigenständiger Wii-Port der PS2Recomp-Laufzeit**, ohne Gekko2 und **ohne BIOS** (`docs/wii/BUILD-AND-STATUS.md`:
„No PS2 BIOS is required"; „Gekko2 is a separate, currently paused project"). Basis: Upstream
`e5636981…` (derselbe Commit, den wir pinnen). Bestandteile:
* **Statisch übersetzter EE-Code**: der gesamte Crash-ELF wird offline vom Upstream-Generator in C++ umgewandelt
  (2.848 Übersetzungseinheiten) und mit devkitPPC zu nativem PPC kompiliert (DOL ≈ 26,7 MB). Es gibt keinen
  Interpreter für EE-Code (Handoff: „Static EE, VU AOT already active (startup zero instruction fallback)").
* **HLE statt BIOS/IOP**: Kernel-/IOP-Dienste sind in C++ nachgebildet (`src/lib/Kernel`, `ps2_iop_host.cpp`).
* **Flaches Speichermodell**: `READ32(addr)` maskiert mit `PS2_RAM_MASK` auf `rdram`, MMIO über `runtime->Load*`.
  Keine TLB-Emulation pro Zugriff.
* **Grobes, kooperatives Timing**: `eeCheckpointDue()` an Rückwärtskanten und `dispatchGuestBranch()` an jedem
  Funktionsübergang (charged `kGuestDispatchCycles`). VU1, GIF, DMA, Audio laufen als Dienste/Threads.
* **VU1-AOT** (`tools/vu/generate_aot.py`) mit einer Issue-Schleife für Hazards/Takt, Interpreter als Orakel.
* **Wii-Plattform** (`ps2xRuntime/platform/wii/`): MEM1/MEM2-Linkerplatzierung (heiße Einheiten in MEM1), skalare 128-Bit-Lanes
  (`ps2_wii_simd.h`, kein SSE), GX-Treiber inkl. resident Z24-Engine (noch ohne Messung auf echter Wii).
Gemessen (nur Dolphin, 60 s Intro, `HYBRID-PERFORMANCE.md`): VU 41,8 %, GS-Raster 35,0 %, DMA-Dienst 12,4 %.
Älterer abgeglichener Lauf (Handoff): VU ≈ 35 %, „native preparation" 23,5 %, Software-GS 20,7 %, Fragment-Permission 8,3 %.
Diese Zahlen sind Wandzeit-Belegung auf Dolphin, **keine** Wii-FPS.

Die beiden Stände sind **nicht** austauschbar: andere Zeitbasis, anderes Speichermodell, andere Kernel-Schicht.

## 2. Wohin geht die Zeit? (Struktur-Analyse, keine Messung)

Entscheidend ist `ee_retire_instruction_inline` (`ee_core.c:5556-5690`). **Jede** retirete Gast-Instruktion führt aus:
`vu0_tick`, Prüfungen für `vu1_tick` und `dma_gif_service`, `COP0 Count++`, `ee_irq_tick`, Timer-Latch,
VBlank-/GS-VSync-Prüfung, `ee_timers_tick_fast`, `sif_ee_tick`, RPC-/CDVD-Pending-Prüfungen, INTC-Latch und die
Interrupt-Entscheidungen, plus BIOS-Sonderfälle (`ee_check_boot_unblock_selfloop`, `ee_check_eeload_fastboot_patch`,
`ee_check_browser_menu_escalation_heuristic`, `ee_check_browser_idle_carousel`, SBUS-Wait). Viele sind per
`service_fast` abgekürzt, werden aber je Instruktion geprüft. Danach `ee_core_interleave_account` (alle 8 Slots wird
der IOP gerechnet).

Vorhandene Messung im Repo (`docs/R1330-I-WII-PERFORMANCE.md`, **gelinkter PPC-Lauf mit gemockten Diensten, Instruktionszählung,
keine Wii-Zeit**): 8 EE-Retirements ≈ 3.318 PPC-Instruktionen; 64 Retirements mit 8 IOP-Grenzen ≈ 26.315 —
also **grob 400 PPC-Instruktionen je Gast-Instruktion** im warmen nativen Pfad. Statisch übersetzter PS2Recomp-Code
liegt konstruktionsbedingt bei einstelligen bis niedrigen zweistelligen PPC-Instruktionen je Gast-Instruktion, ohne
Per-Instruktions-Retire (nur `ctx->pc`-Schreibzugriffe und Funktions-/Kanten-Checkpoints).
Das erklärt die Größenordnung des Faktors ~10 zwischen 2 FPS und 0,2 FPS **als Hypothese** (EE-/Retire-Overhead);
es ist kein Beweis, solange die Zeitanteile nicht auf der Wii gemessen sind.

| Komponente | A19 | PS2Recomp 29.10 | Hypothese / Gewicht |
|---|---|---|---|
| EE-Ausführung | Interpreter bzw. präziser Block-JIT, Retire je Instruktion | statisches natives PPC | **größter Hebel** |
| IOP | echter IOP-Kern, Interleave 8:1, BIOS-Boot | HLE, kein BIOS | hoch (IOP-Idle-Batching ist standardmäßig aus) |
| VU1/VU0 | getaktete Pipeline, optional AOT/JIT | VU-AOT + Issue-Schleife | in PS2Recomp 35–42 % → nach EE-Fix wohl größter Block |
| GS | Software-Raster + GX-Hybrid (`gif.c`) | Software-GS + GX Z24-Engine | 20–35 % in PS2Recomp |
| Scheduler/Events | instruktionsgenau | Checkpoints je ~32 Zyklen/Funktion | Teil des EE-Hebels |
| Speicher | TLB-Übersetzung je Zugriff (`ee_mem_*`), EE-RAM 32 MB per `memalign` (MEM2) | Maske auf `rdram` | MEM2-Latenz trifft beide; Gekko2 zusätzlich Translationskosten |

MEM1 (24 MB) ist bei Gekko2 mit ~20 MB statischem BSS belegt (u. a. Code-Arena 6 MB, GS-VRAM 4 MB, `precise_cache`
2,9 MB, SPU2-RAM 2 MB, VIF je 1 MB); `verify_wii_memory.py` meldet im Standard-Build ≈ 3,1 MB Rest. Das begrenzt Cache-Arena
und jeden zusätzlichen heißen Speicher.

## 3. Was von PS2Recomp übernommen werden kann

**Direkt verwendbar (Werkzeuge/Wissen, offline):**
1. Der Upstream-Generator (`ps2xRecomp`): Funktionserkennung, Sprungtabellen-Analyse, `gif_dma_kick_analyzer`. Er erzeugt
   **architekturneutrales C++** (Typen `R5900Context` mit `__m128i`-Makros). Es gibt **kein PPC-Backend**; der PPC-Code
   entsteht erst durch `devkitPPC-g++` beim Bauen. Das ist die tatsächliche „Zielarchitektur": C++-Quelltext.
2. Die Wii-Plattformschicht aus 29.10 (`ps2_wii_simd.h`, Heap-/Thread-/MEM1-Platzierung, GX-Treiber) als
   Referenz für Grafik- und Laufzeitpfade.
3. Das Prinzip **grober Checkpoints** (Funktionsgrenze + Rückwärtskante statt Retire je Instruktion).
4. Der VU1-AOT-Ansatz (Generator + Interpreter als Orakel) — Gekko2 hat mit `tools/vu_aot_generate.py` ein Gegenstück;
   Vergleich nötig, kein Blind-Port.

**Für PPC750 anzupassen:** skalare 128-Bit-Lanes (in 29.10 vorhanden), Endian-Zugriffe (PPC hat `lwbrx`/`stwbrx`),
Registerpressure der Generatorausgabe (große Funktionen → viele Spills), Codegröße (MEM2-Ausführung, 32-KB-L1).

**Neu zu entwickeln:** ein Block-/Hybrid-Dispatch, der PS2Recomp-Code **unter Gekko2-Zeitbasis** laufen lässt; ein
Retire-Modell mit Event-Horizont statt Retire je Instruktion; fehlerpräzise Speicherzugriffe im nativen Code
(Fast-Path + Seitenausgang); Invalidierung bei selbstmodifizierendem Code; ein Cache-Format für übersetzte Blöcke.
Nicht übernehmbar: die upstream x86/SDL/ImGui-Frontends; die komplette Upstream-Laufzeit als zweite Hardware-Emulation
(würde die in A9 festgehaltene ABI-Trennung `R5900Context*` ↔ `ee_state_t` verletzen und zwei Schedulen erzeugen).

## 4. Hybrid-Ausführungspfad (Vorschlag)

Prinzip: **eine** Hardwarezeitbasis (Gekko2), aber Zeit wird **blockweise** gebucht statt je Instruktion.

1. **Dispatcher** `ee_step_budget()` → `ee_aot_try_execute_fetched()` bleibt der einzige Eintrittspunkt (A8/A15-Vertrag).
2. **Block** = Funktion/Basisblock aus dem Hybrid Manager (Tier 2: offline vom PC erzeugt, Tier 1: vorhandener Block-JIT,
   Tier 0: Interpreter). Jeder Block trägt `n_instr` und Worst-Case-„Zyklen".
3. **Event-Horizont**: `ee_state` hält `slots_to_next_event` (kleinster Abstand zu COP0 Count/Compare, VBlank, Timer,
   DMA/VU1/GIF-Fälligkeit, IOP-Slice). Ein Block darf **vollständig** laufen, wenn `n_instr ≤ horizont`; dann wird einmal gebucht
   (`ee_core_interleave_account(n)`, `COP0 Count += n`, Timer/VBlank bulk). Sonst Fallback auf den präzisen Pfad bis zum Ereignis.
   (Die Grundlage existiert: `ee_core_native_window_begin_aot` für reine Integer-Fenster.)
4. **Speicherzugriffe**: schneller Pfad für direkt abgebildetes RAM (KSEG0/Identity, ggf. DSI-Fastmem `GEKKO2_DSI_FASTMEM`),
   MMIO/Scratchpad/TLB-Miss → Seitenausgang mit exaktem PC/Retire-Zählerstand und Wiedereintritt über den Interpreter.
5. **Exceptions/Interrupts**: nur an Blockgrenzen oder im Seitenausgang; Interrupt-Bereitschaft verkürzt den Horizont
   (Interrupt-Zeitpunkt bleibt auf Block-Granularität, bei kritischen Fenstern präzise).
6. **Selbstmodifizierender Code / Mapping**: vorhandene Mechanismen weiterverwenden (`precise_page_generation`, Mapping-Epoche,
   Live-Wort-Prüfung am Blockeintritt statt je Instruktion; bei Schreibzugriff auf Codeseiten Block-Generation erhöhen).
7. **Caches**: Tier 2 offline (Windows-Manager) erzeugt Quelle/Objekt; Tier 1 on-device; persistente Ablage auf SD
   nur für Tier 1 (PPC-Maschinencode relokierbar) — Tier 2 braucht ein Lade-/Relokationsformat und ist erst Phase 4+.
8. **VU/GS/DMA** bleiben Gekko2-Dienste; ihre Fälligkeiten speisen den Horizont. Keine Änderung der VU-Timing-Korrekturen.

Offline (Windows) sinnvoll: Funktionsfindung/CFG, C++/C-Erzeugung, Kompilierung mit devkitPPC, Kompatibilitätsberichte.
Auf der Wii: Tier 1 (JIT), Cache-Verwaltung, Fallback.

## 5. Beantwortung der Leitfragen

1. **Zusätzlicher Overhead durch die Integration:** per-Instruktion-Retire (§2), 8:1-IOP-Interleave mit echter BIOS-/IOP-Ausführung,
   TLB-Übersetzung je Speicherzugriff, präzise Zeitbasis. 29.10 hat all das nicht.
2. **Läuft PS2Recomp-Code in A19 noch durch Interpreter/Dispatch?** In der hochgeladenen ELF läuft **gar kein** PS2Recomp-Code
   (kein AOT-Symbol). In A19 ist der Pfad kompiliert (`-DGEKKO2_EE_AOT`), die Tabelle aber leer, solange kein privates
   `ee_aot_generated.inc` gebaut wird. Alle A11–A19-Tests sind Host-/Unicorn-Nachweise.
3. **Native Blöcke tatsächlich ausgeführt?** Für diese ELF: nein (EE-Blöcke standardmäßig aus). Für A19 mit Flags: nur der
   präzise Block-/Chain-Pfad; Hybrid-AOT mit realen Spielblöcken wurde nie auf Wii gebaut/gemessen.
4. **CPU-Zeit EE/IOP/VU0/VU1/Scheduler:** Gekko2 hat dafür bereits Zähler (`PROFILE_COST` je Kategorie `EE, IOP, SCHEDULER, GS_RASTER,
   GX_*, VU0, VU1, EE_NATIVE`, sowie `CPU_SAMPLE`, `include/core/runtime_profile.h`). **Diese Logs von der Wii fehlen.** (Plan §7.)
5. **Speicher/Synchronisation Recompiler↔Emulator:** an jeder Blockgrenze Callback + Prüfungen; in A19 je Instruktion
   (`ee_aot_prepare` + `ee_core_block_commit`, außer reinen Integer-Fenstern).
6. **Zuerst optimierbar ohne Grafik-/Stabilitätsverlust:** (a) richtiger Build mit Projekt-Flags, (b) Retire-Bündelung
   auf Blockebene, (c) IOP-Idle-Batching/Event-Scheduler-Optionen (`GEKKO2_OPT_IOP_IDLE_BATCH`, `EVENT_SCHEDULER`, `LONG_EE_BLOCKS`
   — sind heute aus), erst danach VU/GS.

## 6. Die fünf ersten Dateien/Module

| # | Datei | Warum zuerst |
|---|---|---|
| 1 | `source/core/ee/ee_core.c` (`ee_retire_instruction_inline`, `ee_core_interleave_account`, `ee_core_native_window_begin_*`) | Hier liegt der Per-Instruktion-Overhead; Event-Horizont + Block-Buchung müssen hier definiert werden. |
| 2 | `source/core/recompiler/ppc_dynarec.c` (+ `ee_jit.c`) | Emittiert heute je Instruktion Callbacks; braucht Block-Epilog ohne Retire je Instruktion und Seitenausgänge. |
| 3 | `tools/ee_aot_generate.py` | Verallgemeinert das reine Integer-Fenster auf Speicher/Branch/Call mit Block-Buchung; Erzeugungspunkt für Tier 2. |
| 4 | `source/core/recompiler/ee_aot.c` (+ neuer `ee_hybrid_manager.c`) | Eintrittstabelle, Block-Verknüpfung (Direct-Link statt Binärsuche), Cache/Invalidierung, Fallback-Policy. |
| 5 | `source/core/iop/iop_core.c` und Scheduler-Optionen in `optimization.h` | IOP-Interleave/Idle-Batching; ohne dies bleibt der IOP-Anteil auch nach EE-Optimierung hoch. |

VU (`vu.c`, `vu_fmac_internal.h`) und GS (`gif.c`, `gs_gx.c`) folgen erst **nach** Messung; dort liegen die Crash-Korrekturen
(`e7aae58`, SIGNAL-Backpressure) — nicht vereinfachen, nicht ersetzen.

## 7. Messplan (vor jeder Implementierung)

1. **Identität klären:** SHA-256 der DOL/ELF der 0,20-FPS-Messung und der 2-FPS-Messung (29.10), Commit/Quelle, Optionen.
2. **Zwei Builds aus `integration` bauen** (Windows-Project-Hub, devkitPPC r51): (a) `make` Standard, (b) `tools/build_r1389.sh`-Flags
   (EE_AOT, EE_BLOCK_FAST, DSI_FASTMEM, VU_AOT, IOP_AOT, GX_*). Gleiche Einstellungen, gleicher Crash-Savepunkt, kalt gestartet.
3. **Pro Lauf mitschicken:** FPS-Overlay-Wert über ≥ 60 s im Gameplay, `PROFILE_COST …`- und `CPU_SAMPLE …`-Zeilen aus dem SD-Log
   (Kategorien EE/IOP/SCHEDULER/VU0/VU1/GS_RASTER/GX_*), Hash der DOL, Optionsmaske (`GEKKO2_OPTIONS`/Launcher-Profil).
4. Auswertung: Anteil je Kategorie ⇒ erst dann die Reihenfolge der Implementierung festlegen. Referenz 29.10 mit demselben Szenenpunkt.

## 8. Risiken für Crash und Grafik

* **Zeitbasis-Änderung** (Retire-Bündelung) verschiebt VBlank-/DMA-/GIF-/VU1-Interaktionen; die Crash-Korrekturen beruhen genau auf
  diesem Timing (VI-Branch-Bypass, CLIP-Latenz, SIGNAL-Backpressure). Jede Änderung braucht die 92 Crash-Gates + 1024 VU-AOT-Vergleiche.
* **Präzise Exceptions** bei Seitenausgängen (TLB, Adressfehler; Gekko2 löst heute keinen Address-Error bei Misalignment aus — bekannte Lücke).
* **Selbstmodifizierender Code/Overlay-Laden** (Crash lädt Level-Code): Invalidierung muss lückenlos sein.
* **Codegröße/MEM2:** statisch übersetzte Ganzspiele (≈ 27 MB DOL in 29.10) passen nicht in MEM1; Hybrid braucht Block-Auswahl, nicht Totalübersetzung.
* **Upstream-Quirks:** z. B. `blez/bgtz` (A17) werten Upstream-seitig nur 32 Bit aus; Lowering folgt Gekko2.
* **Baseline-Verwechslung** (§0) — Regressionstests nur gegen eine eindeutig identifizierte, gehashte Referenz-DOL.
* **Evidenz:** Host/Unicorn/Dolphin/Wii getrennt führen; Dolphin-Zeiten (29.10) sind nicht auf die Wii übertragbar.
* **Rechtliches/Privates:** kein Spiel-ELF, keine generierten Quellen, keine BIOS/Disc im Repo.

## 9. Nächste Schritte (zur gemeinsamen Entscheidung, nichts davon begonnen)

1. §7-Messung (nur Builds und Logs, keine Quelländerung).
2. Danach Entwurf des Retire-/Event-Horizont-Vertrags (`ee_core.c`) als eigenes Dokument mit Tests, bevor Dynarec/AOT angefasst werden.
3. Optional: `ps2recomp_capability_report.py` (liefert Zahlen akzeptierter/abgelehnter Funktionen ohne Spielcode), damit Tier-2-Prioritäten datenbasiert sind.
