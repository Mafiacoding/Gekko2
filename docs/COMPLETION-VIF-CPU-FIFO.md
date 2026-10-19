# C05a: CPU-SQ auf VIF0/1 und Backpressure

Der CPU-128-Bit-Schreibpfad verwendete direkte void-VIF-Parseraufrufe statt
`vif_accept_quadwords`. Bei I-bit-Stall konnte SQ über die begrenzte
Annahmekapazität hinaus retired werden; der große Parser-Restpuffer war kein
korrekter Ersatz für Producer-Backpressure. Der neue Test scheiterte vor dem
Patch an verändertem PC/Delay-/Retirementzustand des noch nicht angenommenen SQ.

CPU und DMA verwenden nun dieselbe begrenzte qword-Annahme. Vor JIT/AOT/
Interpreterausführung prüft SQ die Annahmefähigkeit. Bei vollem Puffer bleiben
PC, next_pc, Delay-Slot und 128-Bit-Quelle erhalten, während die bestehende
Parktick-Gerätezeit weiterläuft. Kein IRQ-/VIF-Stall wird künstlich gelöscht.
STC ist weiterhin die echte Gastaktion, die einen I-bit-Stall aufheben kann.

Test mit realem I-bit-MARK für VIF0 und VIF1, gefüllter Queue, 200 Wiederholungen,
unveränderten Nutzdaten und genau einmaliger Wiederaufnahme nach FBRST.STC.
Die bestehenden IPU-/GIF-Stall- und VIF-DMA-/Interlocktests bleiben Regressionen.

VIF0 nimmt höchstens acht, VIF1 sechzehn qwords an; die Kapazitäten werden
auch im DMA-Pfad eingehalten und STAT.FQC zeigt die akzeptierte Warteschlange.
Referenz: vendored PCSX2 Vif0_Dma.cpp und Vif1_Dma.cpp (FQC-Grenzen 8/16).
Buslatenzen und vollständige Pipeline-/DMA-Sonderfälle bleiben in C05/C06 offen.
Kein direkter Beleg für einen behobenen Crash-/Tekken-Spielablauf auf Wii.
