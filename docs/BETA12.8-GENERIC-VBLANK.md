# Gekko2 BETA12.8 — übertragbarer EE-VBlank-Schnellpfad

## Befund aus echtem BETA12.7-Wii-Log
- Build `R1389-SPEED-BETA12.7-VBLANK-POLL` bestätigt.
- Bis zum Testende 3.392.525 Fast-Skips und 1.021.812.592 dabei ereignisgesteuert übersprungene EE-Slots.
- VBlank-START wurde 342-mal wirklich signalisiert; die Spiel-CPU beobachtete das Bit 214-mal als gesetzt.
- 22 erfolgreiche FILEIO-Öffnungen, inklusive 3/3 Font-OPEN; 44 Lesevorgänge, 3.235.125 Bytes. Es gab keinen neuen `NuError`.
- `GS_ROUTE draws=10170`, GPU GX ready=1, GS PMODE 0x66, GS-Framebuffer-Zugriffe erfolgen. Letzter angezeigter Puffer bei `FBP=0` ist in der 8x8-Stichprobe schwarz, während die andere Puffer-Stichprobe (FBP=143360) 64/64 nichtschwarze Werte hat.
- Letzte EE-PC = `0x001FC988` in `VSync`; Programmlog meldet `EXIT_HBC` (Benutzerausstieg), keinen emulierten Absturz.
- Mehr als 2 emulierte VBlanks/s in einzelnen Intervallen, Spitzen bis 6,498/s; diese Werte sind **keine** bestätigten Gameplay-FPS.
- Die E3-Demo-CD enthält nicht alle angefragten optionalen LEVELS/B/TITLES-Dateien. Nicht vorhandene Dateien dürfen nicht synthetisch durch beliebige Daten ersetzt werden.

## Änderungen BETA12.8
- Neuer **PC- und spielunabhängiger** MIPS-Musterprüfer für unveränderte INTC VBLANK_START-Warteschleifen: LUI/LW/ANDI4, max. 3 NOPs, korrekt zurückspringendes BEQZ, NOP-Delay-Slot.
- Base- und Zielregister müssen übereinstimmen; nur lesender MMIO-Zugriff auf echtes `0x1000F000`, keine anderen Befehle im Kreis.
- Kein Speed-Skip, wenn IRQ-Bit 2 bereits gesetzt ist; die bisherigen Eventgrenzen, DMA-/SIF-/VU-/IRQ-Guards bleiben unverändert.
- `EE_VBLANK_POLL.last_fast_site` zeigt, an welcher EE-Adresse der generische Fastpath zuletzt erkannt wurde, unabhängig vom Spiel.
- BETA12.6 FILEIO-2110-Pfaderkennung weiter enthalten.

## Regressionen
- Neu: generischer MIPS-Matcher für anderen Guest-PC und GPR; ungültiger Branch, MMIO-Status bereits gesetzt, Memory-Side-Effect sowie Side-Effect im Branch-Delay-Slot werden zurückgewiesen.
- Alter Crash-E3-VBlank-Test: PASS.
- EE-INTC-Interruptlatenz: PASS.
- GS-Output, Low-ELF-JIT, FILEIO-Protokoll: PASS.
- devkitPPC Cross-Compile, ELF/DOL-Check und MEM1 statischer Freispeicher: PASS.

## Nicht gelöst oder nicht auf Wii bestätigt
- Kein nachgewiesener Fix des Crash-E3-Blackscreens oder erstmaliger In-Game-Eintritt.
- BIOS, Tekken, GT3 werden nur dann beschleunigt, wenn ihre tatsächlich ausgeführten Instruktionen dieses strenge VBlank-Pollmuster enthalten; keine pauschale Speedsteigerung versprochen.
- Weitere Ursachenanalyse für den Blackscreen muss VU/GIF-Kommandoreihenfolge, Front-/Backbuffer-Synchronisierung und Ressourcenladepfad vergleichen, ohne ein schwarzes Bild künstlich zu ersetzen.

Keine privaten BIOS-, ELF- oder Spieldaten sind im Paket.
