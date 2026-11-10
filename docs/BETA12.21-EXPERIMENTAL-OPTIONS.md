# BETA12.21 – experimentelle Registerketten und sichere GX-Fences

Arbeitsbranch: `codex/beta12-21-experimental-options`. Basis ist der angeforderte
`codex/ipu-mpeg-accuracy` (`307d694`), einschließlich des separat überprüfbaren
Integrationscommits `80c4e04`. Dessen GS-/VIF-/IPU-Korrekturen bleiben erhalten.
`main` wird nicht verändert. Keine privaten BIOS-, Disc- oder Logdateien im Patch.

## Implementierter 128-Bit-Pfad

Ein eigener Schalter im CPU/GX-Menü: **Y: 128-BIT JIT CHAIN RESIDENCY –
EXPERIMENTAL ON/OFF**. Der bestehende Build-Schalter
`GEKKO2_EE_CHAIN_RESIDENCY` bleibt Voraussetzung. Ohne Fähigkeit zeigt das Menü
UNAVAILABLE; es gibt keine automatische Aktivierung. Standard OFF.
EE-JIT, EE-Blocks, Native-Links und Residency bleiben zusätzliche notwendige
Abhängigkeiten; der neue Schalter schaltet diese nicht heimlich ein.

Die Wunschkonfiguration steht in `sd:/pcsx2/jit-chain.cfg`:

```
GEKKO2_EE_CHAIN_RESIDENCY 0
```

ON verwendet den Wert 1. Speichern ändert nur den Wunschwert. Erst Cold Boot
übernimmt beide experimentellen Einstellungen; Resume verändert sie nicht.
Ungültige/fehlende Dateien führen auf OFF, Schreiben erfolgt über temporäre
Datei/Umbenennung mit Backup. Der vollständig belegte normale 32-Bit-
Optimierungsmaskenraum wird nicht um einen ungültigen 33. Bit-Schalter erweitert.

Der Native-to-Native-Wrapper sichert weiterhin r14..r31; gemeinsame ausgewählte
EE-Lanes liegen in r18..r29. Die bedarfsgewählte Karte bleibt bis zum Cache-Reset
unveränderlich. Jedes ausgewählte GPR/HI/LO-Register umfasst vier 32-Bit-Lanes;
SA ist separat abbildbar. Nicht abgebildete Register bleiben kanonisch gespeichert.
Dies erhält den vollständigen 128-Bit-Gastzustand, bedeutet aber nicht, dass alle
32 EE-GPR gleichzeitig in zwölf verfügbaren PPC-Registern liegen.

Zusätzlich zu geeigneten ALU/MMI-Blöcken sind jetzt geprüft zugelassen:

- LQ/SQ und LD/SD auf durch die bestehenden präzisen Helfer bewiesenem RAM.
- J/JAL, JR/JALR und BEQ/BNE, mit höchstens einem geeigneten Delay Slot.

Speicher- und Kontrollkörper bleiben kanonisch. Danach und an **jedem** Ausstieg
solcher Blöcke werden alle abgebildeten Lanes und die Generation nachgeladen.
Auch ein früher Ausstieg vor dem späteren Körper darf nicht dessen statische
Validitätsannahme übernehmen. Link-Aliasing wird durch den bestehenden Emitter
behandelt. Ein fehlender RAM-/Delay-Helfer lehnt die Übersetzung atomar ab.

Die Runtime-Begleitblöcke verwenden außerdem die vorhandenen fused
Retirement-/Prepare-Grenzen und SPEED-Stubs; der Legacy-Build behält seine
separaten Helfer. Es werden keine IRQs oder Gast-Retirements ausgelassen.

Es bleibt Write-through; keine entfernten EABI-Saves, keine ungesicherten Dirty-
Register. Quellwort-/Codeowner-/Generation-/Mapping-/Budget-/IRQ-/Fault-Prüfungen
und Pinning bleiben bestehen. MMIO, fehlgeschlagene RAM-Beweise, unbekannte
Instruktionen, Merge-/sonstige Speicherformen, Likely-/REGIMM-/COP1-Sprünge und
opake Körper benutzen den bisherigen JIT-/Interpreterpfad. Eine vollständige
Native-only-Abdeckung sämtlicher EE-Instruktionen und Dirty-Writeback sind offen.

Log: `EE_CHAIN_RESIDENCY enabled=... runs=... retired=... successors=...`.
Zusätzlich protokolliert `EXPERIMENTAL_OPTIONS` compiled/requested/active und
Cold-Boot-Semantik; Versions-/Compiler-/Build- und normale Optionswerte bleiben.

## PSM und Logbefunde

**X: EXPERIMENTAL PSM CONVERSION ON/OFF** bleibt erhalten, ebenfalls standardmäßig
OFF. Bestehendes Format `sd:/pcsx2/gs-compat.cfg`:

```
GEKKO2_GS_EXTENDED_PSM 0
```

Kein stilles Einschalten experimenteller Farbtiefenumrechnungen. Die Tests
vergleichen unterstützte gleich tiefe Formattransfers mit normalem GS-Speicher,
prüfen bewusste Cross-Depth-Freigabe, Ablehnungen und Erhalt von CT24-Alpha.
Experimentelle Replikations-/Alpha-Konvertierungsregeln sind nicht als echte
PS2-Hardwaresemantik bestätigt.

Die privat gelieferten Wii-Logs belegen FIRST_IMAGE, aber weder Crash-Gameplay
noch eine 128-Bit-bedingte Exception. Crash hatte sehr wenige native Ketten und
keine ausgewählten Nachfolger. BIOS-GX war aktiv; die fehlenden Kacheln lassen
sich daraus nicht abschließend erklären. Fonts wurden tatsächlich gelesen;
fehlende optionale Dateien werden nicht erfunden. IPU-Ausgaben liefern hier
keinen Beleg für die Ursache des Schwarzbilds.

Nachweisbare Korrekturen aus Code und Diagnosen:

- Raw DTHE=1 bei CT32/CT24 blockiert den GX-/Opaque-Zugang nicht mehr. Dithering
  wirkt dort nicht; CT16/CT16S behalten die korrekte Dither-/Fallback-Semantik.
- GX-Abschluss verlangt einen geordneten PE-Token. Ein alter Callback und ein
  scheinbar inaktiver GP beweisen keinen Abschluss. Token-Wrap wird behandelt.
- Ein Timeout/Abort wird niemals als erfolgreich gerendertes Bild ausgegeben.
  Readback, CPU-Import und Präsentation propagieren Fehler. Sticky Fehler sperrt
  weitere native Arbeit; verlorene Pixel/VRAM-Eigentümer werden nicht erfunden.
  Bei verlorenem GPU-Inhalt kann ein vollständiger App-Neustart erforderlich sein.
  Der alte Diagnosefeldname `recoveries` zählt jetzt Aborts; `GX_FENCE` benennt
  das korrekt. Die eigentliche Ursache jedes realen GPU-Stalls bleibt offen.
- libogc 3.1.0 flushte GX_SetDrawDone bereits; ein fehlender Flush ist keine
  belegte Fehlerursache.

## Reproduktion und Vergleich

```
python3 tools/test_beta12_21_host.py --output-json /tmp/beta12-21-host.json
/workspace/.venv-gekko2/bin/python tools/verify_chain_residency.py
```

Die folgenden Tools erhalten jeweils
`Gekko2-BETA12.21-Options-Chain.elf --nm <devkitPPC>/bin/powerpc-eabi-nm`:

- `verify_chain_residency_linked.py`: gesamte EE-/Timer-/Display-/IRQ-Snapshots,
  Budgets, Quellmutation, Writes, Mapping und Interrupts.
- `verify_chain_alu_linked.py`: skalares ALU-/SPEED-Batching samt Budget-,
  Mapping-/IRQ-Grenzen und tatsächlichem DMA-SMC.
- `verify_chain_memory_linked.py`: vier Lanes LQ/SQ, LD/SD, HI/LO/SA, Daten- und
  Code-RAM, SMC während der Ausführung, TLB-Fehler, Mapping, IRQ und Budgets.
- `verify_chain_branch_linked.py`: J/JAL/JR/JALR/BEQ/BNE, Link-Alias,
  128-Bit-Delay-Slot, Syscall-/TLB-EPC/BD, IRQ und Schedulergrenzen.
- `verify_chain_lane_sentinels.py`: unveränderte obere HI-/LO-/Link-Lanes,
  SA, RAM-Adress-/Zielalias und Sprünge direkt am Blockanfang.
- `verify_beta12_21_modes.py`: vier Kombinationen der beiden Schalter,
  tatsächliches FromSPR-DMA-SMC und unveränderte aktive Werte nach Menüwunsch.
- `verify_gx_fence_beta12_21.py`: tatsächlich verlinkter PPC-Wartepfad mit
  modellierten PE-/Clock-/libogc-Diensten, keine echte GPU.
- `compare_chain_modes.py`: identisches synthetisches EE-Programm, warme Caches,
  OFF/ON-Zählung ausgeführter PPC-Instruktionen und Zustandsvergleich.
  **Keine Wii-Zeitmessung und keine daraus abgeleitete FPS-Steigerung.**

Wii-Vergleich: gleiche BIOS-/Disc-Dateien und normale Einstellungen, jeweils
Cold Boot; zuerst beide OFF, dann nur Chain, nur PSM, dann beide ON. Gleiche
SESSION-Checkpoints vergleichen, requested/active kontrollieren und Bildfehler,
EE-Retirements, VBlank/Present, Chain- und GX-Fence-Zähler getrennt aufzeichnen.
Vorhandenen Rückfall-DOL behalten. Ein fehlerfreier Hostlauf ersetzt dies nicht.

Build: `bash tools/build_beta12_21_options.sh`, Kontrollbuild zusätzlich
`CHAIN_RESIDENCY=0`. devkitPPC r50, GCC16.1.0, libogc3.1.0, Unicorn2.1.4,
-O1 und bisherige SPEED/DSI/AOT/GX/ARM/IDLE/LIGHTREC-Flags erhalten.
Buildverzeichnisse getrennt und für BETA12.21 frisch angelegt. Der primäre Build
enthält die Chain-Fähigkeit, startet jedoch mit beiden Optionen OFF.

## Synthetischer Leistungsvergleich

Das identische warme 128-Instruktions-MMI-Programm liefert exakt gleichen
EE-/Timer-/Display-/IRQ-Zustand. Gemessen mit `compare_chain_modes.py`:

| Modus | Ausgeführte PPC-Instruktionen | Native Nachfolger |
|---|---:|---:|
| OFF, bisheriger normaler JIT | 82.464 | 0 |
| ON, separate Prepare/Commit-Helfer (Zwischenstand) | 90.774 | 14 |
| ON, vorhandene fused Grenzen (ausgelieferter Stand) | 84.264 | 14 |

Damit beseitigt die Anbindung an vorhandene SPEED-Grenzen einen Teil des
zusätzlichen Aufwands. Dieser Test zeigt weiterhin rund **2,18 % mehr** PPC-
Instruktionen als OFF, keine belegte Beschleunigung. Der Modus bleibt deshalb
experimentell und standardmäßig OFF. Instruktionsanzahl berücksichtigt keine
Wii-Caches, Speicherlatenzen oder tatsächliche Laufzeit und ist kein FPS-Wert.

## Grenzen

Keine reale Wii-/Dolphin-Ausführung dieses neuen Builds. Crash-E3-Blackscreen,
SCPH-50004-Kacheln, Tekken-/GT3-Spielbarkeit und reale FPS sind nicht als behoben
oder verbessert bestätigt. Vollständige GS/GX-Parität, universeller Native-ABI,
IPU/MPEG-/VU-Zyklentreue und übrige CDVD/SIF/Peripherie bleiben Entwicklungsarbeit.
Die konkreten Buildhashes und abgeschlossenen Testergebnisse stehen im
beigefügten Artefaktmanifest und im nachfolgenden Teststand.

## Abgeschlossener Teststand

- 27 Host-Suites PASS; darunter Konfigurationsbuilds ohne/mit Chain-Fähigkeit
  sowie Interpreter-only und 21.632 PSM-/Schalterkombinationsfälle ohne Fehler.
- 5.400 ausgeführte PPC-ABI-/128-Bit-/HI-/LO-/SA-Fälle PASS, elf atomare Ablehnungen.
- 105 bisherige verlinkte vollständige EE-/Timer-/Display-/IRQ-Vergleiche PASS,
  126 Nachfolger, auf dem abschließenden fused-Chain-ELF erneut ausgeführt.
- 24 zusätzliche skalare ALU-/SPEED-Batch-Vergleiche PASS, 21 Nachfolger,
  einschließlich tatsächlichem DMA-SMC, Mapping und Scheduler-/IRQ-Grenzen.
- 72 verlinkte Speicher-/SMC-/Budget-/Mapping-/TLB-/IRQ-Vergleiche PASS,
  38 tatsächlich ausgewählte native Nachfolger.
- 2.400 bestehende PPC-Allokator-/Residency-/Alias-/Zero-Oracle-Fälle PASS.
- 32 zusätzliche verlinkte Lane-/Alias-/Blockanfangs-Vergleiche PASS.
- 112 verlinkte Integer-Sprung-/Delay-/EPC-/BD-Vergleiche PASS, 44 Nachfolger.
- 20 verlinkte Vergleiche über alle vier Schalterkombinationen PASS;
  OFF wählt keine Ketten, ON jeweils 15 Nachfolger pro PSM-Konfiguration.
- Verlinkte VIF-DIRECT-Prüfung aller 17 Eingangssplits sowie vier GS-Zielformate
  PASS. GX-Fence-PPC-Oracle PASS: Token ohne IRQ, Ablehnung veralteter
  Callbacks/Idle, Token-Wrap, fehlgeschlagener Abort und Sticky-Fehler.
- Beide Wii-DOL/ELF-Abgleiche und MEM1-Gates PASS. Chain-Build:
  **2.707.276 Bytes MEM1 statisch frei**, DOL SHA-256
  `09b6523f2eedba04de6018fc045e2b445d9f26fbfc58b34c0851ade4a83e254e`.
  Kontrollbuild: **2.843.980 Bytes MEM1 statisch frei**, SHA-256
  `27f2f6e8f0f112d2cdbd5735507a660df3d965baf404d4cfb5bcd4de22698080`.

Diese Zahlen bezeichnen Hosttests bzw. synthetische Ausführung tatsächlich
verlinkten PPC-Codes. Plattform-/Cache-/PE-Dienste sind modelliert. Kein realer
Wii-/Dolphin-Start und kein Beleg vollständiger PS2-Hardwaretreue.
