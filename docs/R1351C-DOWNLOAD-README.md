# Gekko2 R1351C Beta

DSI sitzt jetzt im tatsächlich verwendeten präzisen EE-LW-Blockpfad.
R1351B hatte auf der Wii status=1/probes=4, aber emitted=0. C ergänzt
Block-Zugriffsstellen und die dazugehörige Speicherklassifikation.

apps/Gekko2-R1351C/ nach sd:/apps/ kopieren oder die DOL als boot.dol
verwenden. Einstellungen wie bei B lassen: EE dynarec, Fastmem und
6 MiB code arena einschalten, Emulator neu starten und Cold Boot.
Kein zusätzlicher DSI-Schalter und kein neues IOS nötig.

Im Log prüfen:
- BUILD checkpoint=R1351C
- DSI_PROGRESS status=1
- block_sites und block_prepares müssen während des BIOS-Laufs wachsen.
- faults/patched können bei reinen RAM-Zugriffen0 bleiben: RAM verursacht
  keinen DSI. MMIO/ROM-Zugriffe werden beim ersten Fault zurückgepatcht.

Bitte Zeit bis zum ersten Bild und den vollständigen Log vergleichen.
Ein Wii-Speedgewinn ist für C noch nicht gemessen. Der neue Pfad gilt für
LW-Lesezugriffe; andere Opcodes und nichtfaultende Ereignisfenster behalten
ihre bestehenden Wege. Gast-TLB/unaligned/Store-Prüfungen bleiben erhalten.

Geprüft: tatsächlicher PPC-Code erzeugt DSI-Blockstellen und MMIO-Rückpatches;
20 Vergleiche vollständiger Gast-/Timerzustände für kurze Budgets, rs=rt,
Branch-Delay-MMIO, unaligned und Gast-TLB-Fehler. Native Fenster, Cache-
Präfixe, Scheduler, FPU und Helfer-ABI bestehen ebenfalls. MMU/SPR/Exception-
Auslösung wird im Test modelliert; das ist keine echte Wii-Backpatch-Messung.

Quellcode und Patch B→C liegen bei. Build: bash source/tools/build_r1351c.sh
mit gesetztem DEVKITPRO/DEVKITPPC. Toolchain r50/GCC16.1.0/libogc3.1.0.
Der ARM-Worker ist unverändert; sd-data/pcsx2/arm/ bei Bedarf nach
sd:/pcsx2/arm/ kopieren. ARM-Antwortkorruption und Nullvertices bleiben offen.
Details: source/docs/R1351C-DSI-BLOCKS.md. GPLv3-Lizenz liegt bei.
