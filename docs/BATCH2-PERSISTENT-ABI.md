# Batch 2: experimentelle saubere native Registerübergabe

Basis: Batch-1-Branch bis `b7e0377`, BETA12.20. Kein Release-/Gameplaybeleg.

## Implementiert

Ein zusätzlicher Native-only-Blockeinstieg und eigener C-kompatibler
Kettenwrapper erhalten ausgewählte EE-Wörter in PPC r18..r29 über separat
kompilierte Blöcke. Der Wrapper sichert r14..r31 einmal; r30 trägt die
GPR-Generation, r31 die erwartete Blocklänge. Normale C-Blockeinstiege sichern
weiter wie zuvor. Die Lane-Werte werden durchgehend in den kanonischen
Gastzustand geschrieben, deshalb sind sie bei jedem C-Helfer und Ausstieg
sauber. Es gibt hier noch keine Dirty-Lane-Writeback-Optimierung.

Die unveränderliche gemeinsame Karte unterstützt GPR-, 128-Bit-HI/LO- und
SA-Offsets; GPR0, PC/next_pc, Duplikate und Generation-Aliase sind verboten.
Die experimentelle Laufzeitkarte enthält drei vollständige GPRs ($4/$5/$6).
HI/LO/SA bleiben in dieser Karte kanonisch im Speicher; alternative Karten
mit diesen Offsets werden im PPC-Oracle ausgeführt.

Laufzeitaktivierung ausschließlich mit `GEKKO2_EE_CHAIN_RESIDENCY` beim Build
und bestehenden Native-links-/Residency-Optionen. Default bleibt aus.
Nur überprüfbare gerade ALU/MMI-Körper sind zugelassen. Branches, Speicher-
zugriffe, opake/Helferkörper und unbekannte Instruktionen behalten den
bisherigen ABI-/Interpreterpfad. Fehlende kompatible Nachfolger beenden die
native Kette; es wird nie ein C-Blockeinstieg mit lebenden Gastregistern
angesprungen.

Der vorhandene Resolver bleibt maßgeblich für PC, Owner-Serial, Quellwort,
RAM-Schreib-/Mapping-Generationen, Budget, Interleave und Fault-/Branchzustand.
Die bisherigen Vorbereitungs-/Retirement-Helfer bleiben an jeder Instruktion.
Während der Kette bleiben Codebesitzer gepinnt; Ersatz/Reset geben auch die
neuen Begleitfunktionen erst außerhalb aktiver Ketten frei.
MFSA/MTSA waren bereits im PPC-Emitter enthalten und werden jetzt auch von
normaler präziser Blockbildung zugelassen.

`EE_CHAIN_RESIDENCY` protokolliert enabled/runs/retired/successors. Die
Zähler sind keine Leistungs- oder Hardwarekorrektheitsbelege.

## Prüfungen und Reproduktion

- `tools/verify_chain_residency.py`: 1.800 ausgeführte synthetische PPC-Fälle,
  unabhängiges 128-Bit-Bitoperator-/HI/LO/SA-Oracle, saubere Helfersicht,
  vorzeitige Ausstiege, Generationsüberlauf, alle nichtflüchtigen Register.
  Unveränderte Karten werden nach dem Wrapperladen nicht erneut gelesen.
  11 ungültige Karten/ungeeignete Körper werden ohne Ausgabe abgewiesen.
- `tools/verify_accuracy_allocator_r1384.py`: 2.400 vorhandene ausgeführte
  PPC-Allokator-/Alias-/Residency-Vergleiche PASS.
- Cache-Generationen R1316, Eviction R1318, EE-Exceptions,
  BETA12.17-Residenzplan und Live-Mapping R1305 PASS.
- `test_chain_residency_cache`: Companion-Owner, aktive Pins,
  Ersatz und Reset PASS.
- `test_beta12_16_codegen`: 500.015 Checks, 0 Fehler. Separat mit
  ppc_dynarec.c/ppc_code_cache.c/optimization.c verlinken; der generische
  Host-Runner lässt den PPC-Emitter standardmäßig weg.

Python-PPC-Checks benötigen Unicorn 2.1.4. Ausführen:

```sh
/workspace/.venv-gekko2/bin/python tools/verify_chain_residency.py
/workspace/.venv-gekko2/bin/python tools/verify_accuracy_allocator_r1384.py
```

Der zusätzliche `tools/verify_chain_residency_linked.py` prüft den tatsächlich
verlinkten Dispatcher/EE-Retirement gegen den Interpreter mit Quellmutation,
notifizierten Writes, Mapping-Epoch, Budgets und Compare-IRQ. Er benötigt den
experimentellen ELF und `--nm <devkitPPC>/bin/powerpc-eabi-nm` samt benachbartem
GCC/objcopy. Die Fixture leitet die private Slotstruktur vom jeweiligen Build
ab, einschließlich des experimentellen Flags. Plattformallokation/Cache-
operationen werden modelliert; dies ist keine Wii-Ausführung.

Wii-Build: `bash tools/build_batch2_chain.sh`, frisches `build-batch2-chain`.
Toolchain: devkitPPC r50/GCC16.1.0/libogc3.1.0; -O1 und bestehende
SPEED/DSI/AOT/GX/IDLE/LIGHTREC-Flags erhalten. Experimenteller ELF/DOL-Abgleich
PASS; statisch 2.714.700 Bytes MEM1 frei. DOL SHA-256:
`7ae04ff94348d457c62169e20c0ae69ee80d9dd57f3ca922c077d52842aa68cc`.
Zusätzlicher Kontrollbuild ohne Flag ebenfalls strukturell geprüft,
2.849.228 Bytes MEM1 frei. Keine reale Wii-/Dolphin-Ausführung.

## Weiter offen

Kein allgemeines profilgesteuertes Mapping, kein Dirty-Writeback, keine
Native-only-Branch-/Speicher-/opaken Körper, keine behauptete Beschleunigung.
Vollständige DMA/SMC/TLB/ASID-/Eviction-/IRQ-Differentialabdeckung muss weiter
wachsen, bevor eine Default-Aktivierung erwogen werden darf. Aktueller
Codecache und vorbereitete Instruktionsgrenzen bleiben zwingend. Kein
Nachweis der Crash-/Tekken-/GT3-Spielbarkeit.

Abgeschlossene verlinkte Prüfung: `verify_chain_residency_linked.py` meldet
`PASS 105 linked interpreter/native-residency complete EE/timer/display/IRQ comparisons; selected successors 126`.
Budgets 1..128 inklusive Grenzwerten, unnotifizierte erste/innere Quellmutation,
notifizierter innerer Write, Mapping-Generation und Compare-IRQ werden geprüft.
Das bleibt synthetische PPC-Ausführung mit modellierten Plattformdiensten.
