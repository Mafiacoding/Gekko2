# Gekko2 BETA12.10 – TRXDIR=2 Local-to-Local, abgesichert

## Stand
`TRXDIR=2` war in BETA12.9 bereits implementiert: GS-Registernachrichten aus
`BITBLTBUF`, `TRXPOS` und `TRXREG` lösen `gs_mem_copy_local` aus, ohne eine
IMAGE-Payload abzuwarten. Unterstützt wird der vorhandene Same-PSM-Pixelpfad.

## Gegenüber BETA12.9 konkret verbessert
- Vor dem Transfer wird ausstehender GX-residenter VRAM sicher auf den CPU-Schatten
  synchronisiert. Misslingt das Readback, meldet der Transfer einen Fehler statt
  erfolgreiches Kopieren zu behaupten.
- Bei identischer GS-Oberfläche und geometrischer Überlappung bestimmt der
  Transfer die nötige Kopierrichtung zum Schutz der Quelldaten, selbst wenn
  `DIRX`/`DIRY` versehentlich nicht gesetzt wurden. Für andere Oberflächen
  bleiben die Richtungsvorgaben des Spiels unverändert.
- Der vorhandene `GS_LOCAL_COPY`-Zähler protokolliert weiterhin erfolgreiche
  Transfers, abgelehnte Transfers und kopierte Pixel.

## Host-Verifikation
- PSMCT32 zwischen getrennten Bereichen; PSMCT32-Überlappung rechts/unten
- Überlappung rechts/unten bei `DIRX=0` bzw. `DIRY=0`
- PSMCT24 ohne Veränderung des vorhandenen Alpha-Bytes
- PSMCT16S und PSMT8 mit echten GS-Swizzle-Adressierungen
- GPU-Resolver-Fehler: Transfer wird abgelehnt, GPU bleibt Pending
- GIF A+D Registerintegration, `XDIR=2` führt Transfer aus und setzt `XDIR=3`
Alle Tests PASS. Wii-Cross-Build mit devkitPPC 16.1.0 / libogc 3.1.0 PASS.
DOL-/ELF-Abgleich PASS. Statische MEM1-Prüfung PASS (2,936,588 Byte Rest).

## Einschränkungen
- Unbekannte bzw. abweichende PSM-Konvertierungen werden weiterhin abgelehnt.
  Nicht alle PCSX2 `Move()`-Pfade sind damit portiert.
- VRAM-Adresswrap / alle physisch überlappenden Aliase / `TRXDIR=1` sind offen.
- Dieser Stand ist nicht auf echter Wii getestet. Es ist **kein** Nachweis, dass
  Crash E3 den GS-Move benötigt oder damit schon ins Spiel kommt.
- Diagnosebuild `-O1`; nicht direkt mit älteren `-O3` FPS vergleichen.

## Wii-Test
Im Log muss `R1389-SPEED-BETA12.10-GS-MOVE-SAFE` stehen.
`GS_LOCAL_COPY transfers=... declines=... pixels=...` verrät, ob das Spiel
`TRXDIR=2` nutzt. Wenn `transfers=0`, erklärt dieser Transferpfad nicht
unmittelbar den jeweiligen Crash-Blackscreen.
