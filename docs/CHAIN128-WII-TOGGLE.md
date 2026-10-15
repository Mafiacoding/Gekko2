# Experimentelle 128-Bit-EE-Blockverkettung: Wii-Menüschalter

Basis: Codex-Branch `codex/ipu-mpeg-accuracy` (über alle fünf Batches) plus eigener Branch `feature/chain128-wii-toggle`.

## Zweck / Bedienung

Auf der Wii: **Settings → CPU/GX-Optimierungen → Y**. Zusätzlich zum vorhandenen **X** für experimentelle GS-PSM-Konvertierung erscheint eine eigene Zeile:

- `Y: 128-BIT JIT CHAIN OFF (SAFE)` — Standard und Rückfallweg.
- `Y: 128-BIT JIT CHAIN ON (EXPERIMENTAL)` — aktiviert nach dem **nächsten Cold Boot**, nicht bei START/Resume.
- `Y: ... UNAVAILABLE` — die DOL wurde ohne `GEKKO2_EE_CHAIN_RESIDENCY` erstellt.

Die Auswahl steht getrennt vom vollständig belegten 32-Bit-Optionsmaskenformat in `sd:/pcsx2/chain128.cfg` als `GEKKO2_CHAIN128 0|1`. Datei fehlt/ungültig → AUS. Nur der nächste Cold Boot wendet sie an; pausierte Sessions behalten den bisherigen Zustand. Für die tatsächliche native Kettenausführung müssen auch `EE dynarec`, `EE native blocks`, `Native block links` und `GPR residency` aktiv sein.

Das Menü aktiviert **nicht** pauschal 128-Bit-JIT für alle Instruktionen. Codex implementierte einen kleinen geprüften Native-to-Native-ABI für gerade ALU/MMI-Blöcke; Instruktionen mit Speicher-, Branch-, Exception- oder Hilfsaufrufrisiken bleiben auf den bisherigen Pfaden. Dirty-Lane-Writeback und allgemeine pro Block dynamische Karten fehlen. Die Register werden im Gastzustand write-through gehalten; Standardpfad bleibt unverändert.

## Build

Der neue Build `bash tools/build_chain128_toggle.sh` verwendet devkitPPC/libogc und alle bisherigen SPEED/DSI/AOT/GX/IDLE/LIGHTREC-Flags **plus** `-DGEKKO2_EE_CHAIN_RESIDENCY`. Ergebnis: `Gekko2-CHAIN128-TOGGLE.dol` / `.elf`.

Die Option im Menü wird auch in DOLs ohne Flag angezeigt, ist dort aber **nicht auswählbar**. Bitte nicht einfach alte 12.20-DOL verwenden: dort fehlt der Codex-ABI-Compilerpfad.

## Tests und Bewertung

Host-Control-Smoke-Test:
```sh
cc -std=c11 -Wall -Wextra -O2 -Iinclude \
  tests/test_chain128_runtime_toggle.c source/core/recompiler/optimization.c -o /tmp/chain128-off
/tmp/chain128-off
cc -std=c11 -Wall -Wextra -O2 -Iinclude -DGEKKO2_EE_CHAIN_RESIDENCY \
  tests/test_chain128_runtime_toggle.c source/core/recompiler/optimization.c -o /tmp/chain128-on
/tmp/chain128-on
```
Beide Kontrollpfade wurden gegen dieselbe Schalterlogik lokal erfolgreich getestet. Für den branchgenauen Build und PPC-Code dient die ergänzte GitHub-Actions-CI.

Beim Wii-Test im Session-/Startlog `CHAIN128` und `EE_CHAIN_RESIDENCY enabled=... runs=... retired=... successors=...` prüfen. **enabled=1** allein bedeutet nicht, dass eine native Kette tatsächlich ausgeführt wurde; `runs` und `successors` müssen anwachsen.

Testplan: Crash-E3-Demo zweimal von demselben Disc-/BIOS-/GX-Setup cold-booten, erst OFF, dann ON. Logs und sichtbare Ausgabepuffer/FPS vergleichen. Bei Crash/FPS-Rückgang sofort OFF und Cold Boot. Ein schnellerer JIT löst nicht automatisch das bisherige 32-Bit-Framebuffer-/Asset-Problem; **Crash-Blackscreen bleibt unbestätigt**.

Keine privaten PS2-Images, BIOS oder Logs in Repository/CI. Neue Menüfunktion ist experimentell und wird vor Wii-Hardwaretest nicht in `main` übernommen.
