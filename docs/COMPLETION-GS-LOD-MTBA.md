# C02: GS Q/K/L, Mipmap-Filter und MTBA

Bisher wählte Gekko2 ein einziges Mip aus dem Verhältnis Texturgröße zur
geclippten Bildschirm-Boundingbox. Dadurch konnte bereits ein SCISSOR-Wechsel
eine andere Textur auswählen. L wurde ignoriert; Mip-Texelkoordinaten und
Wrap-/Clamp-Dimensionen blieben auf Basisebene. MTBA hatte keinen Adresspfad.

Jetzt berechnet der Softwarepfad LOD aus K/16 und, bei STQ/LCM=0,
`-log2(abs(Q)) * (1<<L)`. Dreiecke benutzen interpoliertes Q je Fragment,
Sprites das Q der zweiten Ecke. UV verwendet konstantes K. Die sechs gültigen
MMIN-Modi wählen nearest/bilinear und gerundetes Mip/trilinear, MMAG greift
bei Vergrößerung. MXL ist auf die sechs vorhandenen Mipregister begrenzt.
Koordinaten, TW/TH und Region-/Repeat-/Clamp-Grenzen werden auf Mipebene skaliert,
kanonische Register nach jedem Sample wiederhergestellt.

TEX0-Schreibzugriffe erzeugen bei gültigem MTBA-Kontext die ersten drei
Mip-Adressen aus quadratischer TW-Fläche und physischer Formattiefe. TH/TBW
und MXL beeinflussen diese Autoadressen nicht. Mindestabstand ein Block,
TBP-14-Bit-Wrap, beide Kontexte und ausbleibender TEX2-Trigger werden geprüft.
MIPTBP2 bleibt explizit; MTBA wird nicht als sechs automatisch erzeugte Mips ausgegeben.

Die vorhandenen GX- und Texelcache-Envelopes decken bisher nur die Basisebene
ab. Mipmapped Draws benutzen deshalb den gemeinsamen korrekten Softwarepfad;
alle bestehenden Nicht-Mipmap-GX-Optimierungen bleiben verfügbar. Keine
unvollständige GPU-Texturcache-Zulassung nur zum Erzwingen von GX.

Verhaltensreferenzen: gepinnte PCSX2-GSState.cpp GIFRegHandlerTEX0/GetTex0Layer,
GSRendererSW.cpp und GSDrawScanline.cpp im bestehenden Referenzbaum bzw.
`091ab68708a555198d80a7216965481c7ddd0d98`. Eigenständiges C-Sampling;
kein PCSX2-Rasterizer importiert. Exakte GS-Logarithmus-/Float-Bitpräzision,
AA1 und vollständige GPU-Mipmapgleichheit bleiben offen.

Gezielt erfolgreich: neuer Test mit **11.701 unabhängigen Layout-/Farb-/Zustands-
und Rasterfällen**, darunter alle sechs Filter, negative Koordinaten,
trilineare Zwischenwerte, Q als Zweierpotenzen, beide MTBA-Kontexte,
TEX2/Adressenwrap und wirklich unterschiedlich ausgewählte Mips im selben
Dreieck. Scissor ändert die Q-Werte unveränderter Pixel nicht.
Bisherige Sprite-/Dreieck-/Kontext-Mip- und UV-Bilineartests bestehen ebenfalls.
Alte Solid-Color-Fixtures enthielten absichtlich die bisherige Bildschirmgrößen-
Heuristik; sie setzen nun explizites konstantes K für ihren UV-Zweck.
Die neue variable-Q-Oracle ersetzt diese Heuristikannahme, keine Assertion wird entfernt.

Kein Nachweis korrigierter BIOS-/Tekken-Kacheln oder Crash-Spielbarkeit auf Wii.
