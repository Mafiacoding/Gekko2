# Gekko2 R1363 Sound Beta

R1363 integriert den vorhandenen ADPCM-Mixer erstmals in einen Gast-Zeitplan mit optionalem Wii-Audioausgang. Zusätzlich schließt sie konkrete SPU2-Pfade, auf deren Status und IRQs Spiele warten können. Dies ist keine vollständige Implementierung aller PS2-Spielmodule.

## Änderungen gegenüber R1362

- 48-kHz-Stereo aus der vorhandenen IOP-Zeitrechnung: ein Sample pro 768 Scheduler-Ticks. Diese Ticks bleiben die bisherige Instruktions-/Zyklus-Näherung des Emulators; kein Anspruch auf vollständig genaue PS2-Zeitrechnung.
- SPU2-Adresseinheiten korrigiert: 16-Bit-Wörter statt der fälschlich vom PS1-SPU übernommenen Acht-Byte-Einheiten.
- DMA4 bedient Core0, DMA7 Core1. Normaler Upload und Rücktransfer, 20-Bit-Adress-Wrap, TSA-Fortschreibung, MADR-Fortschreibung und DMA-Abschluss.
- Direkte FIFO-Zugriffe über TDATA lesen/schreiben jetzt wirklich SPU2-RAM.
- Zwei getrennte Core-Master-Volumes mit dem richtigen Abstand von 0x28 statt eines gemeinsamen Master-Volumes. VMIX-/MMIX-Routing und Core0-Ausgabe als Core1-Eingang.
- Native 16-Bit-Stereo-AutoDMA: 256 linke Samples, dann 256 rechte Samples je Datenblock; zwei RAM-Hälften und Nachfüllen. Abschluss nach Verbrauch des letzten Samples statt sofort nach dem Upload; STATX-Transferstatus folgt dem laufenden Transfer. Partialblöcke, AC3/Bypass und CDDA32 werden ausdrücklich als nicht unterstützt gezählt.
- ENVX, aktuelle feste Voice-/Master-Volumes, NAX-Lesewerte, programmierte Loop-Adresse nach Key-On und ENDX aktualisiert. Manuelles Umsetzen einer aktiven NAX-Adresse ist noch nicht vollständig implementiert.
- SPU-RAM-IRQs für DMA/FIFO/ADPCM und AutoDMA. Silent Mixer-/Capture-/Ausgabe-Rückschreibungen verwenden die in PCSX2 dokumentierten Pufferadressen und können ebenfalls IRQ9 auslösen. IOP-Idle-Batches überschreiten diese Sample-Grenze nicht.
- Optionaler Wii-DMA-Ausgang, 48 kHz, zwei 32-Byte-aligned Seiten; Interrupt-Callback kopiert nur PCM. Kein Decoder oder wartender Thread im Callback. Ein begrenzter PCM-Puffer verwirft Überlauf und füllt Unterlauf mit Stille.
- Pause, Cold Boot und Exit stoppen die Host-Ausgabe und leeren die Queue. Fortsetzen startet die Ausgabe erneut.
- Checkpoints speichern SPU2-RAM, Register, Stimmen, ADPCM-Historie, Envelope/Pitch, Zeitphase und laufende AutoDMA-Beschreibung. Host-Audio-Queue wird nicht gespeichert. Ältere Checkpoints ohne Sound-Blöcke erhalten einen definierten stummen Sound-Reset.
- Neuer persistenter Schalter `SPU2 sound output (beta)`. Optionsformat Version9; ältere gespeicherte Optionen lassen ihn ausgeschaltet. Der Schalter betrifft den Host-Ausgang; die Gast-Hardwarestatus-/IRQ-Fortschreibung bleibt aktiv.
- `SPU2_AUDIO` im Log: frames, KON, DMA, IRQ, ADMA, unsupported, queued, submitted, played, dropped, underrun, nonzero und running.
- Der VF00-Geometriefix und die feinere Codearena-Suche aus R1361/R1362 bleiben enthalten. Keine GX- oder CPU-Taktänderung in dieser Runde.

## Tests

- 32 portable Regressionstests bestanden: neuer Sound-/Queue-/IRQ-/Options-Test, aktiver Sound-Checkpoint, SPU2-Register/Mixer/DMA, bestehende IPU-Decoder-/RSPU-Streaming-/Checkpoint-, VU-, Pad-, DMA-, Lightrec- und Cache-Tests. Der Cache-Zufallstest umfasst 100.000 Operationen.
- Die PowerPC-Version im ELF liefert die erwarteten 768 AutoDMA-Stereo-Samples über beide RAM-Hälften hinweg; DMA-Abschluss erfolgt beim letzten Sample. Der Test startet kein Audio-Gerät.
- Silent Mixer-Ausgabe löst den programmierten SPU-IRQ auf der ersten 768-Tick-Grenze aus.
- PowerPC-EE-Präfixe unter Arena-Druck, 26 EE/IOP/Timer-Vergleiche und sechs SIF-Barrierefälle bestanden.
- DOL-Sektionen stimmen byteweise mit ELF überein; Wii-Startup-Signaturen vorhanden.
- Mixer-Objekt enthält keine externen 64-Bit-Multiplikations-/Divisions-Helfer für die Sample-Mischung.
- Gebaut mit devkitPPC GCC16.1.0 / libogc3.1.0, ARM-Beta, DSI-Fastmem und bestehendem Lightrec-IOP-Pilot.

Hörbare Ausgabe auf einer echten Wii, ein neuer Tekken-Lauf und ein Geschwindigkeitsgewinn sind noch nicht bestätigt. CPU-/PPC-Tests ersetzen den Hardwaretest nicht.

## Wii-Test

1. R1363 starten. In den Optimierungen den letzten Schalter `SPU2 sound output (beta)` aktivieren; er wird gespeichert. Danach Cold Boot.
2. BIOS bis zum ersten Bild laufen lassen. Anschließend Tekken kalt starten und bis zur Titel-/Charakterauswahl prüfen.
3. Vollständiges Log senden. `nonzero>0` zeigt produzierte hörbare PCM-Samples, `played>0` den Verbrauch durch den Host. Große `underrun`-Zahlen sind bei deutlich zu langsamer Gast-Ausführung erwartbar. `unsupported` betrifft nicht unterstützte AutoDMA-Modi/Geometrien.
4. Ein Vergleichslauf mit Sound-Ausgang OFF misst dessen Host-Kosten. Die SPU-Hardwarefortschreibung bleibt auch dann aktiv, damit Spiele ihre Audio-IRQs erhalten.

Bei der aktuellen Geschwindigkeit ist lückenhafter Ton zu erwarten; die Beta erfindet keine fehlenden Samples und beschleunigt den Gast nicht anhand der Host-Audio-Uhr.

## Noch offen

Reverb, Noise, PMON, Sweep-Volumes, Gaussian-Interpolation, vollständige VU/GS-Spielekompatibilität, vollständige MPEG-/FMV-Audio-Pfade und proprietäre Sound-Streaming-Protokolle. Der RSPU2-HLE-Pfad hält bisher rohe Audio-Präfixe bereit; diese Beta deutet sie nicht ohne verifizierten Formatvertrag als Stimmen um. Bestehende native Sound-IRX-Module können die verbesserten Hardwarepfade nutzen, erhalten aber keine pauschale Kompatibilitätsgarantie.

## Wii U / c2w

Dies bleibt ein Wii-DOL, kein nativer Wii-U-/Cemu-Build. Ein realer vWii-Test ist noch nicht bestätigt. Cemu emuliert Wii-U-Software; sein Verhalten ist kein Wii-DOL-Kompatibilitätsnachweis.

FIX94 dokumentiert für ein gepatchtes OSv1-cafe2wii-Image 1,215GHz im Wii-Modus. `sign_c2w_patcher` allein hebt den Takt nicht an: Es leitet den Wii-VC-Start auf das passende gepatchte Image um. 1,215GHz gegenüber 729MHz sind rechnerisch 1,667-facher CPU-Takt, keine garantierte 1,667-fache Framerate. Dieser Build installiert oder patcht keine Systemtitel. vWii/c2w und Wii sollten getrennte Vergleichsläufe erhalten.

## Primärquellen

- [PCSX2 Registertabelle](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/SPU2/RegTable.cpp)
- [PCSX2 SPU2-Registersemantik](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/SPU2/spu2sys.cpp)
- [PCSX2 Mixer und Ausgabe-/Capture-IRQ-Puffer](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/SPU2/Mixer.cpp)
- [PCSX2 SPU2-DMA](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/SPU2/Dma.cpp)
- [PCSX2 Stereo-Eingang](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/SPU2/ReadInput.cpp)
- [PCSX2 DMA4/DMA7-Zuordnung](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/IopDma.cpp)
- [ps2sdk Sound-Registerverträge](https://github.com/ps2dev/ps2sdk/blob/master/common/include/libsd-common.h)
- [libogc Audio-DMA-API](https://github.com/devkitPro/libogc/blob/master/gc/ogc/audio.h)
- [FIX94 c2w_patcher](https://github.com/FIX94/c2w_patcher)
- [FIX94 sign_c2w_patcher](https://github.com/FIX94/sign_c2w_patcher)
