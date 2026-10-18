# SPU2 Noise, PMON und Lautstärke

Vorher ignorierte der PCM-Pfad NON, PMON und Lautstärke-Sweeps. Nun gibt es
je Core eine gemeinsame Noisequelle, Maskenauswahl je Stimme, Modulation
mit dem Post-ADSR-Sample der vorigen Stimme und aktuelle Voice-/Mastervolumes.
Stimme 0 wird nie moduliert. Normales und moduliertes Pitch wird auf 3fffh begrenzt.
Sweeps laufen auch für nicht aktive Stimmen. VOLX wird vom Gerät erzeugt;
feste Lautstärkeschreibzugriffe veröffentlichen sofort den verdoppelten signierten
15-Bit-Wert, Gastschreibzugriffe auf VOLX werden ignoriert.

Verhaltensvergleich: PCSX2 `091ab68708a555198d80a7216965481c7ddd0d98`,
`pcsx2/SPU2/{Mixer,ADSR,spu2sys}.cpp` (GPL-3.0+). Eigenständiger C-Pfad;
Noise-Orakel benutzt die referenzierte 64er Wahrheitstabelle statt des
Produktions-XNOR-Ausdrucks. PSX-SPX
`037c2c4f626841ceb8d421755ea0e81380fa438e` dient nur als zweiter,
PS1-spezifischer Dokumentationsvergleich. PS1- und PS2-Genauigkeit sind
nicht gleichgesetzt. Insbesondere fehlen physische Belege für Grenzraten,
negative exponentielle Sweeps und die komplette SPU2-Buszeit.

Der vorhandene 8192-Byte-Mixerbereich im SPUS-Checkpoint reicht weiterhin aus.
Die alte Voice-Struktur und ihr Prefix bleiben unverändert. Der Loader akzeptiert
die bisherige Größe und setzt dann Noise-/Sweep-Phasen auf Kaltstartwerte;
neue Snapshots behalten diese Phasen, validieren Erweiterungskennung und
Zähler und passen weiterhin in dasselbe äußere SPUS-Format.

Gezielte Host-Regressionen: 9 Tests erfolgreich, davon neuer Kontrolltest mit
**4.196.416 Checks**, 64 Noise-Takten über jeweils 65536 Schritte, Pitchgrenzen,
linearen/exponentiellen Sweeps, aktiven NON/PMON-Capturepfaden, Stimme-0-Schutz,
inaktiven Sweeps und Legacy-/neuen Mixer-Snapshots. Weiterhin erfolgreich:
Mixer, Register, IOP-MMIO, DMA, RSPU-Streaming, RSPU-Checkpoint,
PS2-Karten-Checkpoint und PS2-FAT-Dateien. Keine BIOS-/Spieldaten verwendet.

Reproduktion:
```
python3 tools/build_host_core.py test_completion_spu2 test_spu2_regs test_spu2_mixer test_iop_spu2 test_iop_dma_spu2 test_checkpoint_rspu2_r1340 test_ps2_mc_checkpoint test_ps2_mc_files
# Die ausgegebenen Programme ausführen.
gcc -O3 -Iinclude tests/test_rspu2_stream_r1340.c source/hw/rspu2_stream.c -o /tmp/test-rspu
/tmp/test-rspu
```

Offen bleiben Gaussian-Interpolation, Reverb, vollständiges ADSR/ENVX-/Key-On-
und Silent-Voice-Timing. Dies ist kein Nachweis einer behobenen Crash-/Tekken-
Grafik oder höherer Wii-FPS. Integrierter Wii-Build wird separat protokolliert.
