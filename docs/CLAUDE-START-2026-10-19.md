# Claude: aktueller Einstieg und Arbeitsauftrag

Stand: 19. Oktober 2026. Verwende Branch **`codex/loading-refresh-regression`**,
nicht den älteren GitHub-main. Runtime-Quellcommit: **`51a9259`**. Spätere
Übergabe-/Dokumentationscommits ändern diese Runtime nicht. Der vollständige
Archivcommit steht zusätzlich in `SOURCE-COMMIT.json` des lokalen Quellpakets.
GitHub selbst erzeugte Source-ZIPs enthalten diese zusätzliche Datei nicht;
deren Commit ergibt sich aus der Download-URL.

## Diesen Auftrag an Claude übergeben

> Arbeite am beigefügten Gekko2-Quellstand. Lies zuerst vollständig AGENTS.md,
> CLAUDE.md, docs/CODEX_HANDOFF.md und STATUS.md; dann
> docs/LOADING-REFRESH-REGRESSION.md, docs/GX-BOOT-ARCHIVE-REVIEW.md,
> docs/ALPHA-READINESS-2026-10-19.md und die dort relevanten technischen Berichte.
> Priorität: den Bootstopp der Crash-E3-Demo vor der ersten Bildausgabe lokal
> reproduzieren, mit Interpreter/JIT kontrolliert vergleichen und nur bewiesene
> Fehler beheben. Danach den getrennten seltenen BIOS-GX-Timeout untersuchen.
> Tekken ist ebenfalls gemeldet, aber nicht in dem zuletzt geprüften Logarchiv.
> Bewahre die Ladebildschirmkorrektur, bestehende Optimierungen, präzise
> Exceptions/Interrupts/SIF/DMA und Software-/Interpreter-Fallbacks.
> Entwickle auf einem separaten Branch, führe passende Regressionen aus, baue
> mit aktuellem offiziellem devkitPro/devkitPPC und libogc und liefere einen PR
> sowie einen nachvollziehbaren Testbericht. Keine Spielbarkeit/FPS/Kachel-
> oder Hardwarekorrektur ohne entsprechenden Nachweis. Kein neues Wii-Log
> allein auf Verdacht anfordern: zuerst vorhandene Belege und lokale Tests
> auswerten. Private BIOS-/Disc-/Logdaten nicht ins Repository aufnehmen.

## Was fertig ist

- Ladebildschirm statt erzwungenem Diagnose-HUD; gespeicherter Menüschalter
  **Settings → On-screen logs**, standardmäßig OFF. SD-/Netzwerklogs separat.
- Flackerursache im Loader korrigiert: statischer Hintergrund/Logo/Text werden
  nicht wiederholt im sichtbaren XFB gelöscht und aufgebaut. Kleine animierte
  Streifen bleiben im selben XFB; keine zusätzliche Vollbild-Allokation.
- Regression gegen ausgelieferten `33b057f`: identischer Aufruf erzeugt
  163.353 XFB-Schreibzugriffe und schlägt fehl. Korrigierter `852600c`:
  identischer Aufruf null Schreibzugriffe; Animation/Zeit nur in Dirty-Strips.
- Neue Sessiondateien enthalten den kompilierten Source-Commit und die
  Chain-Buildvariante. VBlank-Delta ist als Intervall markiert, Gesamtsumme
  zusätzlich vorhanden. Diese Diagnoseergänzungen gehören zu `51a9259`.
- README auf main ist über reinen Doku-PR #52 aufgeräumt. Emulatoränderungen
  sind ungemergt in konsolidiertem Draft #51, dieser Folgebranch in Draft #53.
  31 enthaltene alte Drafts geschlossen; Branches/Downloads erhalten.
  Andere offene PRs sind nicht als enthalten nachgewiesen: nicht löschen.

## Was die privaten Logs tatsächlich zeigen

Zehn Dateien wurden lokal geprüft; die Rohdateien sind **nicht** im Quellpaket.
Die folgenden Zahlen sind eine Zusammenfassung, kein nachspielbarer Gastdump.

- Vier Crash-Dateisitzungen, vom Nutzer als **E3-Demo** bestätigt: drei
  BETA12.32, eine BETA12.33. Alle bleiben vor dem ersten Bild bei PMODE=0.
- Aktueller Crash-Lauf: rund 427,4 Mio. EE-Retirements, 53,4 Mio. IOP-Ticks,
  beide CPUs nicht angehalten; GIF 97 Qwords, VU1 startet nicht. Kein GX-Draw,
  keine Gastbildkopie, kein GX-Wait und kein GX-Timeout in diesem Lauf.
- VBlank läuft: 86 Ereignisse über alle Messintervalle. Der alte
  GAME_WAIT.vblank_events-Wert ist ein Intervall-Delta; letzte Null bedeutet
  **nicht**, dass die Uhr nie lief. 30 tatsächliche gelinkte PPC-Grenztests
  mit PMODE=0 bestehen für Interpreter/JIT/native Fenster.
- Ein fehlgeschlagenes Öffnen einer Fehlerdatei, noch keine Font-Öffnung/Reads.
  Nicht bewiesen, dass das fehlende Fehlerfile den Stopp verursacht.
- Separater BIOS-Lauf: 655.467 GX-Waits, 655.466 Finish-Callbacks, ein Timeout
  mit Abort und sicherem Pausieren. Timeout-Register-/Token-Begleitdatei fehlt
  im Archiv. Ursache offen; keine Sicherheitsbarriere entfernen.
- Kein Tekken-gekennzeichneter Lauf in diesem Archiv. Dateinamen sind keine
  verifizierte Disc-Identität.

## Was noch fehlt / nicht behauptet werden darf

Die exakte Crash-E3-Disc liegt hier nicht lokal vor. Der Nutzer will sie später
über einen privaten Link bereitstellen. Vorhanden waren privat sein BIOS und
GT2000 Japan Taikenban. **GT2000 ist weder Crash noch die GT3-Vollversion.**
Alle Privatdateien bleiben außerhalb von Git und dieses Source-ZIPs; Claude
muss für eine exakte Reproduktion separat Zugriff auf erlaubte Nutzerdaten
bekommen. Ohne die Disc ist der Crash-Stopp nicht lokal nachgestellt oder
behoben. Es gibt keine neue physische Wii-Abnahme der Loaderkorrektur.
BIOS-Kacheln, Tekken/GT3-Spielbarkeit und belastbare Wii-FPS bleiben offen.

Keine Pixel erfinden, PMODE/IRQ/PC erzwingen, leere Datei als Erfolg ausgeben
oder abortierte GPU-Arbeit als erfolgreiches Gastbild veröffentlichen.

## Bauen und testen

[Build-Anleitung](BUILDING.md), [Benutzeranleitung](USER-GUIDE.md).
**Aktuell offizielle devkitPro/devkitPPC und libogc** verwenden; aufgezeichnet
getestet: devkitPPC r50, GCC16.1.0, libogc3.1.0. Kein altes r32/libogc1.8-SDK.
SDK, Unicorn und private Assets sind nicht Bestandteil des Quellpakets.

```sh
export DEVKITPRO=/opt/devkitpro # an Installation anpassen
export DEVKITPPC="$DEVKITPRO/devkitPPC"
export PATH="$DEVKITPPC/bin:$DEVKITPRO/tools/bin:$PATH"
CHAIN_RESIDENCY=0 bash tools/build_beta12_33_windows.sh
python3 tools/test_alpha_compat_host.py --output-json /tmp/gekko2-host.json
# Python mit Unicorn für die tatsächlichen ELF-PPC-Prüfungen verwenden:
python3 tools/verify_loading_screen_linked.py Gekko2-BETA12.33-GX.elf --nm "$DEVKITPPC/bin/powerpc-eabi-nm"
python3 tools/verify_frontend_console_linked.py Gekko2-BETA12.33-GX.elf --nm "$DEVKITPPC/bin/powerpc-eabi-nm"
python3 tools/verify_display_edges_linked.py Gekko2-BETA12.33-GX.elf --nm "$DEVKITPPC/bin/powerpc-eabi-nm"
```

Buildskript erhält SPEED/AOT/DSI/GX-Fence-Flags. Commitabhängiges frisches
Objektverzeichnis; bei Flagwechsel am selben Commit keine alten Objekte nutzen.
Kontrollbuild enthält kein Chain-Buildflag. `CHAIN_RESIDENCY=1` ist ein separater
Vergleich; experimentelle Optionen bleiben standardmäßig OFF. Vorhandene
Nutzereinstellungen können ON sein und werden nicht heimlich überschrieben.

Testbelege nicht vermischen: 90 Hostgruppen und neun PPC-Gruppen gehören zum
früheren Frontendstand (siehe ALPHA-FRONTEND-CLEANUP.md und JSON).
`852600c`: Lade-/Fehlerbilder 480/576, Dirty-Strips, 96 Konsolenprüfungen und
30 Displaygrenzen geprüft. `51a9259`: zusätzlich Session-ID-Tests mit
Standard-/explizitem Commit sowie Chain/Control; frischer Wii-Kontrollbuild,
DOL/ELF-Identität und statisches MEM1 bestehen (2.335.084 Bytes Rest).
Das sind keine neuen Wii-/Dolphin-Läufe und keine realen FPS-Ergebnisse.

## Wichtige Dateien

- `source/main.c`: Loader, Dirty-Strips/Invalidierung, First-Image-Gate,
  GX/Software-Ausgabe, Sitzung/Profilzeilen.
- `source/hw/frontend_log.c`: exklusive Sessiondateien und Build-ID.
- `source/core/ee/ee_core.c`: EE-Interpreter/Retirement, unabhängige Displayuhr,
  RPC/CDVD, sichere native Zeitfenster; Änderungen hier sind risikoreich.
- `source/core/recompiler/ee_jit.c`, `ppc_dynarec.c`: PPC-Übersetzung und ABI.
- `source/hw/gs_gx.c`: geordnete PE-Token, bounded fence, sichere Abort-/Pause-
  Behandlung. Nicht ohne Beleg in ungesicherten Betrieb wechseln.
- `docs/GX-BOOT-ARCHIVE-REVIEW.md`: aktueller Belegstand.
- `docs/ALPHA-READINESS-2026-10-19.md`: weitere Alpha-Blocker.

Die 23 anfänglich lokalen third_party-Änderungen waren nur CRLF-Unterschiede.
Sie wurden nicht gestaged/committed. Das Quellarchiv enthält nur getrackte
Commitdateien, keine lokalen Build-/Privatverzeichnisse.
