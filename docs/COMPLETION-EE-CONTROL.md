# Completion-Batch: EE-CF/BC2 und RI

Katalog EE-01/02/03, Basis a976a04. C.F.S fehlte als gültiger R5900-Opcode;
Interpreter, Single-Emitter, präzise Blockpolicy und AOT-Klassifikation sind
jetzt ergänzt. Es löscht nur FCR31.C und liest keine Gleitkommaoperanden.

BC2F/T/FL/TL verwenden native Single-Fragmente mit vorhandener Branch-/Annul-
Semantik. Der skalare Fetch behält VU0-Interlocks; allgemeine Block-/Chain-
Zulassung für COP2-Kontrollsprünge wird damit nicht erweitert. Unbekannte
EE-/COP-/MMI-Dispatchfälle liefern RI (Code10) mit EPC/BD/BEV/EXL statt den
Hostemulator anzuhalten. Bereits modellierte Sonderfälle reservierter PMFHL-
Modi bleiben unverändert; kein pauschaler Reserved-Bit-Hardwarebeweis.

Nachweis: 117 synthetische C.F.S-/RI-Fälle plus Nested-EXL, bisherige BC2-/TLB-
und 6.144 Trap-/IRQ-Prioritätsproben bestanden. 352 unabhängig erwartete,
ausgeführte PPC-C.F.S-/BC2-Fälle vergleichen den gesamten Speicherzustand,
Sign-Offsets, PC-Wrap und EABI; 28 ungültige BC2-Selektoren werden abgewiesen.
Die neue C.F.S-Zulassung wird vom maschinenlesbaren Audit erfasst. MMI- und
bestehende Macro-/Single-Gate-Audits bleiben zusätzliche Grenzen.

Keine echte Wii-/Dolphin-Ausführung, keine behauptete neue Spielbarkeit/FPS.
Der integrierte Wii-Build und produktive gelinkte Dispatcher werden im
abschließenden Batch separat geprüft. Restliche Einträge bleiben im Katalog.
