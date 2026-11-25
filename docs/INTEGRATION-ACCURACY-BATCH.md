# Integrationsbatch auf Basis von 307d694

Branch `codex/integration-accuracy-batch`; main bleibt unverändert.

Implementiert: GS-CT16/16S-Zielfarben werden für Blending durch Linksverschieben
expandiert, nicht durch Bit-Replikation. Dithering gilt nur für CT16/16S;
COLCLAMP folgt dem Dither-Schritt. VIF1 DIRECT leitet vollständige ankommende
Quadwords sofort an PATH2 weiter, bewahrt Teilwörter und SIGNAL-/DIRECTHL-
Rückstau und meldet den Abschluss-IRQ erst nach dem letzten akzeptierten QWC.
Der experimentelle JIT wählt komplette GPR-/HI-/LO-Gruppen und SA anhand der
emittierten Zugriffe des ersten geeigneten Blocks. Die Karte bleibt bis zum
vollständigen Cache-Reset unverändert. Write-through, EABI-Saves und alle
bisherigen Resolver-/Helper-Grenzen bleiben bestehen; kein Dirty-Writeback.

Nachweise dieses Laufs:

- 24 Host-Suites PASS (JSON im Artefaktverzeichnis).
- GS-Pixeloracle: 32.768 Fälle, vorher 7.514 Abweichungen, jetzt 0.
- VIF DIRECT: 81 Checks inklusive aller 17 Wort-Splits, früher Sichtbarkeit,
  SIGNAL-Retry, Folgecode, I-Bit und IMM=0. Der erste fokussierte endgültige
  17-Check-Test hatte vorher 5 Fehler.
- MPEG1/2-Escapes, Nicht-Intra/Intra, beide Scans, inverse Quantisierung und
  wiederholter Eingabestau: 12.192 Fälle, 0 Fehler, plus zwei Fehlerströme.
  IDCT wird dort als Koeffizientenbeobachter ersetzt; kein Filmplayer-Beleg.
- Neue Kartenwahl: ganze GPR20/21/22-, HI/LO-/SA-Gruppen und acht atomare
  Ablehnungen. 5.400 emulierte PPC-ABI-Fälle PASS, elf weitere Ablehnungen.
- Verlinkter PPC: 105 vollständige EE/Timer/Display/IRQ-Differentialfälle,
  126 Nachfolger; zusätzlich zwölf Karten-/Mapping-/echte FromSPR-DMA-SMC-
  Vergleiche, 34 Nachfolger. Verlinkte VIF-Wortsplits und vier GS-Zielformate
  bestanden. Plattformdienste sind modelliert, keine Wii-Ausführung.
- ASan/UBSan für neue GS-/VIF-Pfade sowie MPEG-Koeffizientenoracle PASS;
  bei GS/VIF wurden gif.c/vif.c/gs_mem.c instrumentiert, übrige Hostobjekte
  aus dem normalen Cache verwendet. Leak-Prüfung deaktiviert.

Build: `bash tools/build_integration_accuracy.sh`; experimentell zusätzlich
`CHAIN_RESIDENCY=1`. Getrennte frische Buildverzeichnisse. devkitPPC r50,
GCC16.1.0, libogc3.1.0, bisherige SPEED/DSI/AOT/GX/IDLE/LIGHTREC-Flags und O1.
Beide ELF/DOL-Abgleiche und MEM1-Gates PASS. Kontroll-DOL:
`0012ce52913b9cd127dca9f06b291345469c44a676561183ea4e675850a5fb73`,
2.847.564 Bytes MEM1 frei. Experimenteller Chain-DOL:
`13bae016e29d82d2462d4798ad95921f73a87c03a612338e662a59649ced05ec`,
2.711.884 Bytes MEM1 frei. BETA12.20-Diagnose, keine Releasefreigabe.

Reproduktion: tools/verify_chain_residency.py,
tools/verify_chain_residency_linked.py, tools/verify_chain_demand_linked.py,
tools/verify_integration_paths.py. Linked-Tools benötigen ELF und --nm;
Unicorn 2.1.4. Die Koeffizientensuite separat mit ipu_mpeg.c linken,
Kartenwahl mit ppc_dynarec.c/ppc_code_cache.c/optimization.c. Der historische
VIF-Payload-Test inkludiert ../source/hw/vif.c und wird allein mit
Funktionssektionen und --gc-sections gebaut (16.777.216 Fälle PASS).

Offen bleiben tatsächliche SCPH-50004-/Crash-/Tekken-Läufe, vollständige
GS/GX-Parität, allgemeines Dirty-/Branch-/Memory-Chain-ABI, übrige VU-/IPU-
Zyklentreue und CDVD/SIF/Memory-Card/SPU2-Vollständigkeit. Diese Punkte werden
hier nicht als abgeschlossen ausgegeben. Der nachfolgende konkrete Auftrag
für BETA12.21 erhält eine eigene, von codex/ipu-mpeg-accuracy abgeleitete
Branch; GS-/VIF-Änderungen dieses Batches bleiben getrennt reviewbar.
