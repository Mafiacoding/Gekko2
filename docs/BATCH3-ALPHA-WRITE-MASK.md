# Batch 3: AFAIL=RGB_ONLY / FBA-Schreibmaske

Basis: gestapelter Batch-2-Branch; separate Änderung an GS-Pixelverarbeitung.

Reproduziert: Bei fehlgeschlagenem Alpha-Test mit AFAIL=RGB_ONLY, CT32,
FBA=1 und Zielalpha ohne gesetztes Bit 7 wurde dessen Alpha-Bit trotzdem
geändert. Sowohl Software-GS als auch GX-CPU-Import enthielten dieselbe
falsche Reihenfolge; reine Parität zwischen ihnen hätte den Fehler übersehen.

Unabhängiges Schreibmasken-Oracle vor der Änderung: **4096 Fälle, 32 Fehler**.
Beispiel: Ziel `0x0a556677`, Quell-RGB `0x332211`, RGB_ONLY/FBA=1 ergab
`0x8a332211` statt `0x0a332211`.

Jetzt schützt RGB_ONLY die komplette CT32-Alpha nach FBA über die finale
Schreibmaske. RGB-/Z-Freigaben, FBMSK und CT24-Alpha-Erhalt bleiben getrennt.
CT16/16S behalten die bisherige RGB_ONLY->FB_ONLY-Regel. Keine zusätzliche
GX-Hardwarezustandskombination wird ohne Oracle aktiviert.

Referenzprüfung: PCSX2 `GSDrawScanline.cpp`, TestAlpha/AFAIL_RGB_ONLY maskiert
Alpha, FBA setzt Quellalpha vor der abschließenden Zielmaskierung.
https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS/Renderers/SW/GSDrawScanline.cpp
Eigenständige Korrektur der Schreibmaske, keine Übernahme des Renderers.
Die CT24-DATE-Regel wird bewusst beibehalten: Die aktuelle Hardware-Referenz
beschreibt PS2-Tests mit vollständiger Verwerfung; die ältere SW-Referenz
allein wäre hier irreführend.
https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS/Renderers/HW/GSRendererHW.cpp

Tatsächlich ausgeführt: test_afail_fba_oracle **4096/0 Fehler**,
test_beta12_20_gx_alpha **159/0**, test_gx_pipeline_r1298 **324/0**,
test_beta12_19_framebuffer16 **16/0**, test_date_r1378 PASS.
Das bisherige BETA12.20-Test-Soll für RGB_ONLY+FBA wurde entsprechend korrigiert.
Host-Build/Runner: tools/build_host_core.py, GCC14.2.0.

Wii: tools/build_batch3_alpha.sh, frisches build-batch3-alpha,
devkitPPC r50/GCC16.1.0/libogc3.1.0, bisherige Flags/-O1 erhalten,
experimentelle Chain-Residenz standardmäßig AUS.
ELF/DOL-Abgleich PASS; statisch 2.849.164 Bytes MEM1 frei.
DOL SHA-256: `1e0759573e92f735fd922368390cb5195fdfa6ba41981ed0020b3eb13e5377a0`.
Keine Wii-/Dolphin-Ausführung und kein Crash-Gameplaybeleg.

Offen bleiben unter anderem vollständige GX-TEV-/Blend-/Feedback-/Dither-
Parität und Hardware-Pixelvergleiche. Dieser Schritt vervollständigt nicht
den gesamten GS/GX-Renderer.
