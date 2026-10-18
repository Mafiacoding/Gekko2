# BETA12.29 – Core completion, erster Umsetzungsstand

Basis BETA12.28 `7681af2`; Hauptbranch weiterhin BETA12.20 `ca16340`.
Der Wunsch nach vollständiger Umsetzung wird als fortlaufendes Inventar mit
prüfbaren Einzelbatches geführt. Die Version macht den Emulator nicht vollständig.

- 81 getrennt bewertete Aufgaben in [COMPLETION-CATALOG.md](COMPLETION-CATALOG.md),
  maschinenlesbare Quellen-/Opcode-Aufnahme im Audit. Vorhandene native MMI-
  und VU-Pfade werden von absichtlichen skalaren Sicherheitsbarrieren getrennt.
- [EE-Control](COMPLETION-EE-CONTROL.md): C.F.S, native einzelne BC2-Sprünge,
  Reserved-Instruction-Ausnahmen mit EPC/BD statt Emulator-Halt.
- [SPU2-Datapath](COMPLETION-SPU2-DATAPATH.md): Noise/NON, PMON, Voice-/Master-
  Sweeps und kompatible Checkpoints. Gaussian, Reverb und vollständiges
  Silent-Voice-/ENVX-/Key-On-Timing bleiben offen.
- [GETENTSPACE](COMPLETION-MC-ENTSPACE.md): tatsächliche freie Directory-Einträge.
  Raw-Page, destruktives CheckBlock, weitere Ports und MagicGate bleiben offen.

128-Bit-Chain bleibt experimentell und standardmäßig OFF; EABI-Saves,
kanonischer Zustand und DMA/TLB/IRQ/Budget-/Invalidierungsgrenzen bleiben erhalten.
Kein allgemeines Dirty-Writeback hinzugefügt. Renderer und Optionen bleiben
auf der bisherigen Baseline; kein spezifischer Crash-/Tekken-Grafikfix behauptet.

Reproduktion in frischen Git-Archiven:
```
python3 tools/test_beta12_29_host.py --output-json /tmp/beta12-29-host.json
/workspace/.venv-gekko2/bin/python tools/verify_completion_ee_control.py
bash tools/build_beta12_29_completion.sh
CHAIN_RESIDENCY=0 bash tools/build_beta12_29_completion.sh
```

## Nachgewiesene Ergebnisse

- 70/70 Host-Suites bestanden, einschließlich der vollständigen bisherigen 62.
- Neuer EE-Control-Test: 117 Fälle plus verschachteltes EXL; bestehende
  6144 Trap-/IRQ-Proben und Exceptions-/COP2-/AOT-Regressionsprüfungen grün.
- 352 unabhängig ausgeführte PPC-C.F.S/BC2-Fälle und 28 Reserved-Declines.
- In beiden tatsächlichen Wii-ELFs: je 98 komplette EE-Zustandsvergleiche, davon je 30 native
  Single-Fragmente; RI mit beiden Vektoren und Delay-EPC/BD geprüft.
- SPU2: 4.196.428 synthetische Checks; gezielte Audio-/DMA-/Checkpoint-Suites
  und GETENTSPACE-/bisherige FAT-/READ-ABI-Suite erfolgreich.
- Beide frischen Wii-Builds mit offiziellem devkitPPC r50/GCC16.1.0/libogc3.1.0.
  Gleiche SPEED/DSI/AOT/GX-/Idle-Flags wie BETA12.28; nur der Kontrollbuild
  kompiliert Chain aus. Alle drei Runtime-Experimente bleiben ab Werk OFF.
- DOL-/ELF-Sektionsbytes stimmen in beiden Builds überein. Statisches MEM1-Gate
  bestanden; dies misst weder dynamische MEM2-Spitzen noch Laufzeitstabilität.

| Build | DOL-Bytes | Statisches MEM1-Ende | MEM1-Rest |
|---|---:|---|---:|
| Chain kompiliert, Runtime OFF | 2080280 | `0x815d8d74` | 2257548 |
| Ohne Chain | 2074712 | `0x815b7774` | 2394252 |

Emulator-Buildcommit: `9fe55bf`. Nachfolgende Berichts-/Paketänderungen ändern
keine Emulatorsemantik. Exakte DOL-/ELF-Hashes stehen im JSON/Manifest.
Die bestehenden Chain-/Speicher-/Sprung-/Modus-/GX-PPC-Gates laufen separat;
ihre tatsächlichen Resultate werden vor Paketfreigabe ergänzt.
Keine neuen physischen Wii-/Dolphin-Tests. SCPH-50004-Kacheln, steuerbare
Crash-/Tekken-Spielbarkeit, dynamischer MEM2-Peak und Wii-Speed bleiben unbelegt.
