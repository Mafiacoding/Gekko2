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
python3 tools/test_beta12_29_linked.py Gekko2-BETA12.29-GX-Chain.elf --control-elf Gekko2-BETA12.29-GX.elf --nm "$DEVKITPPC/bin/powerpc-eabi-nm" --output-json /tmp/beta12-29-linked.json
```

## Nachgewiesene Ergebnisse

- 70/70 Host-Suites bestanden, einschließlich der vollständigen bisherigen 62.
- Neuer EE-Control-Test: 117 Fälle plus verschachteltes EXL; bestehende
  6144 Trap-/IRQ-Proben und Exceptions-/COP2-/AOT-Regressionsprüfungen grün.
- 352 unabhängig ausgeführte PPC-C.F.S/BC2-Fälle und 28 Reserved-Declines.
- In beiden tatsächlichen Wii-ELFs: je 98 komplette EE-Zustandsvergleiche, davon je 30 native
  Single-Fragmente; RI mit beiden Vektoren und Delay-EPC/BD geprüft.
- Weitere 14 gelinkte C.F.S/MMI-Blockvergleiche mit Chain OFF/ON, ganzen
  128-Bit-Zuständen, EABI und Budgetgrenzen; 18 native Nachfolger erreicht.
- Vier Options-Metadatenprofile respektieren weiterhin gespeichertes All-OFF.
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
Insgesamt **12 gelinkte PPC-Suites bestanden**: 105 komplette Interpreter-/Chain-
Zustandsvergleiche, 24 ALU-, 72 Speicher-/TLB-/DMA-SMC-, 112 Sprung-/IRQ-/
Scheduler-Vergleiche, 16 Zustände für alle acht Experimentkombinationen,
208 GX-Format-Texel, 53 Big-Endian-GS-Speicherprüfungen sowie XGKICK-Reject/
Ringwrap, CSM2, 32 Tiefentexel, IPU-Reset, Eingabe, sieben VIF-Barrieren und
16 fraktionale UV-/Dreieckfälle. Hinzu kommen die oben genannten neuen
EE-Single- und C.F.S-Blockprüfungen. GX-/Plattformdienste sind in diesen
Unicorn-Fixtures simuliert, keine reale Wii-GPU-Ausführung.

DOL SHA256 Chain: `f189f58b0a435f6e5df4b9437bfaaee8e6246527e4ae42bb651d57cd4bc8a58c`.
DOL SHA256 Kontrolle: `1f3f9c7d6bb819ee14769287ba26bc94d95d5de290f28ec5fd1f1bb590b8f330`.
Maschinenlesbare Zusammenfassung: [BETA12.29-RESULTS.json](verification/BETA12.29-RESULTS.json).
Compile-Warnungen bestehen weiterhin, insbesondere ältere Lightrec/Lightning-
und libogc-Toolchain-Warnungen; kein warnungsfreier Build behauptet.
Keine neuen physischen Wii-/Dolphin-Tests. SCPH-50004-Kacheln, steuerbare
Crash-/Tekken-Spielbarkeit, dynamischer MEM2-Peak und Wii-Speed bleiben unbelegt.


Historische Testabgrenzung: `verify_beta12_23_linked.py` erwartete noch den
4:3-Nachbearbeitungssymbol `gs_xfb_fit_4_3`. Die automatische Anwendung wurde
auf Nutzerwunsch in BETA12.24a zurückgenommen; der unbenutzte Helfer ist im
heutigen ELF nicht verlinkt. Nach 7 erfolgreichen VIF- und 16 UV-Fällen meldete
dieser historische Test deshalb `KeyError`. Der neue `verify_vif_uv_linked.py`
prüft dieselben produktiven VIF-/UV-Fälle und das Fehlen des veralteten Helfers.
Kein Renderer-/Größen-Code wurde für den historischen Test wieder aktiviert.
Der ursprüngliche Fehlbericht bleibt im Reviewpaket nachvollziehbar.
