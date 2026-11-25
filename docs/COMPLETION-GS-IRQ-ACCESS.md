# C01: GS-Registerzugriffe und pending IMR

Vorher: EE LB/LH/LW/SB/SH/SW erreichten GS-CSR/IMR/SIGLBLID nicht; nur
LD/SD wurden geroutet. Ein SW zum Quittieren ließ CSR.SIGNAL unverändert.
Der neue Test scheiterte vor dem Patch bereits am tatsächlichen LW-Leseweg
(`ee_mem_read32(...) & 1`). Alle 8/16/32-Bit-Lanes erreichen nun denselben
GS-Zustand. RAM-Lesefastpfade und bestehende Watchhooks bleiben erhalten.
CSR-Schreibzugriffe werden als W1C-Aktion behandelt, nicht mit gelesenen
unbeteiligten Ereignissen zusammengeführt. IMR-1->0-Maskenübergänge liefern
für bereits gesetzte CSR-Ereignisse einen GS-Interrupt; identische Werte
lösen keinen zusätzlichen Interrupt aus. IMR-Schreibwerte maskieren reservierte
Bits und setzen die feststehenden Bits13/14 entsprechend der Referenz.

Referenz: PCSX2 `091ab68708a555198d80a7216965481c7ddd0d98`,
`pcsx2/GS.cpp`, IMRwrite und gsWrite8/16/32/64_page_01. Eigenständige
lane-basierte C-Implementierung, kein importierter GS-Backend.

Gezielte Tests: neue EE-Alias-/Zugriffsbreiten-/W1C-/SIGLBLID-Regression,
32768 Kombinationen aus CSR-Events und alter/neuer IMR-Maske sowie echte
SB/SH/SW/SD-Instruktionen. Bisherige SIGNAL-PATH1/2/3-/DMA-/XGKICK-
Backpressure- und GS-SIGNAL-Tests bestehen weiterhin. Die zweite SIGNAL-
Nachricht bleibt am Producer, bis der Gast CSR wirklich quittiert.

Dies sind reproduzierte Corefehler, kein Nachweis, dass die konkrete Crash-E3-
Sitzung genau diese Zugriffe benutzt oder dass deren Blackscreen behoben ist.
Vollständiges GS-Reset-/CSR-Readback-/Bus-Timing bleibt getrennt zu bearbeiten.


Privater BETA12.24-Logabgleich: die Sitzung mit Crash-Datei-I/O-Merkmalen
enthält einen nicht wieder aufgenommenen SIGNAL-Wartepfad; in der vorhandenen
Tracehistorie erscheint keine CSR-ACK-Transition. Der Ring meldet jedoch
verlorene Ereignisse. Das Fehlen einer ACK-Zeile beweist daher weder den
konkreten Schreibopcode noch die alleinige Ursache. Die andere Sitzung zeigt
keinen SIGNAL-Wartepfad; ihre Grafikfehler werden nicht mit diesem Interruptfix
erklärt. Rohlogs, Register-/Instruktionswerte und private Pfade bleiben außerhalb
von Git, PRs und Paketen. Der öffentliche Test verwendet ausschließlich eigene
synthetische Pakete und EE-Instruktionen.
