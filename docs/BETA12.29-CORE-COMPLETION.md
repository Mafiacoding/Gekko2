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

Build-/Integrationsresultate werden nach tatsächlicher Ausführung ergänzt.
Keine neuen physischen Wii-/Dolphin-Tests. SCPH-50004-Kacheln, steuerbare
Crash-/Tekken-Spielbarkeit, dynamischer MEM2-Peak und Wii-Speed bleiben unbelegt.
