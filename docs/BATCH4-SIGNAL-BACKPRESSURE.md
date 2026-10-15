# Batch 4: begrenzte SIGNAL-Rückstau-Behandlung

Der produktive `gif_try_process_quadwords`-Pfad akzeptierte ein zweites SIGNAL
bei bereits gesetztem CSR.SIGNAL und überschrieb SIGID/erzeugte weitere IRQs,
ohne auf die Bestätigung des ersten Signals zu warten.

Jetzt bleibt dieses PACKED-A+D-Quadword unakzeptiert beim Produzenten. Der
Parserindex, das Restpaket und der Pfadbesitzer bleiben erhalten; spätere
Zeichenbefehle werden nicht vorgezogen. Nach CSR-W1C kann derselbe Speicher
von DMA/VIF/VU erneut angeboten werden, ohne ein SIGNAL doppelt auszuführen.
SIGNAL/LABEL spiegeln ihre Maskenupdates außerdem nach GS.SIGLBLID, damit
CPU-MMIO-Lesezugriffe dieselben IDs wie der GIF-Zustand sehen.

Es gibt hier bewusst **keine behauptete exakte physische SIGNAL-Queue oder
FIFO-Latenz**: Der synchrone Konsument übt Rückstau vor Annahme des zweiten
Quadwords aus. Der alte void-Embedding-/Testparser hat keinen Accept/Retry-
Vertrag und bleibt unverändert. Produktion verwendet den geprüften Try-Pfad.
`GIF_SIGNAL_STALL` zählt blockierte Aufrufe, keine emulierten Taktzyklen.
Keine serialisierten Zustandsstrukturen wurden erweitert.

Verhaltensreferenz: PCSX2 Gif_Unit.cpp/Gif_HandlerAD, zweites SIGNAL muss bis
CSR-Bestätigung zurückgestellt bleiben. Eigenständige Umsetzung im bereits
vorhandenen Gekko2-Backpressure-Modell:
https://github.com/PCSX2/pcsx2/blob/master/pcsx2/Gif_Unit.cpp

Host GCC14.2.0, tatsächlich ausgeführt:

- test_gif_signal_backpressure PASS: PATH1/2/3, unveränderte zweite ID vor
  ACK, exakte IRQ-Anzahl, maskierte IRQs, SIGLBLID-MMIO, echte normale GIF-DMA
  mit MADR/QWC/Completion sowie VU1-XGKICK-E-Drain und Wiederaufnahme.
- test_hardware_accuracy_r1384 PASS: FMAC-/FDIV-/FSSET-Timing, M,
  GIF-Arbitration und fortsetzbare DMA.
- test_xgkick_timing_aot3 PASS.
- test_gif_register_carry PASS.
- test_gs_signal PASS (alter direkter Register-Testpfad).
- test_vif_split_irq_beta11 PASS.

Wii-Build: tools/build_batch4_signal.sh, frisches build-batch4-signal;
devkitPPC r50/GCC16.1.0/libogc3.1.0, bisherige Flags und -O1 erhalten,
Native-Chain-Residenz AUS. ELF/DOL-Abgleich PASS; statisch 2.848.684 Bytes
MEM1 frei. Keine Wii-/Dolphin-Ausführung, keine FPS-/Gameplaybehauptung.

Offen bleiben exakte SIGNAL-Queue-/FIFO-/Buslatenz, vollständige VIF-/VU-
Pipeline- und Scheduler-Treue sowie reale Spiel-/IRQ-Traces. Dieser Schritt
ist kein vollständiger Ersatz des EE-/IOP-Schedulers.
