# C09a: Writable ENVX

ENVX-Schreibzugriffe wurden zuvor nur im Registerarray gespeichert. Der Mixer
verwendete weiter seinen alten ADSR-Pegel und überschrieb ENVX beim nächsten
Sample. Nun übernimmt der interne Pegel den vollständigen 16-Bit-Registerwert,
ohne Phase/Zähler zurückzusetzen und ohne eine gestoppte Stimme zu starten.
Referenz: PCSX2 spu2sys.cpp Voice Params, `ADSR.Value = value`,
`091ab68708a555198d80a7216965481c7ddd0d98`.

Große manuelle ENVX-Werte können Post-ADSR-OutX über signed16 hinausheben.
PMON behandelt diese Faktoren ohne unsigned Overflow und mit korrekter
0..3fff-Sättigung. Eine frühe Sättigung hält die restlichen Produkte in 32 Bit.
Das bestehende Checkpointlayout bleibt unverändert; Validatoren akzeptieren
nun auch die gültigen manuell geschriebenen Pegel bis ffffh.

Erfolgreich: 8230 synthetische Fälle über alle 48 Stimmen, unveränderte
Phase/Zähler, Checkpointroundtrips, tatsächliche Capture-Gainwerte und ein
unabhängiges signed64-PMON-Orakel. Bisherige 4196428 Noise/PMON/Sweep-/SPUS-
Checks, Mixer- und Systemcheckpointtests bleiben grün.
Gaussian, Reverb, genauer Key-On und Silent-Voice-/Bus-Timing bleiben offen.
