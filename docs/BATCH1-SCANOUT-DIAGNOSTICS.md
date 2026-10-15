# Batch 1, Schritt 1: Crash-E3-Scanout-Diagnose

Basis: GitHub main `ca16340e14381cb29a1b108c2553b3f35d71ab40`,
Codeversion BETA12.20. Branch `codex/batch1-scanout-diagnostics`.

## Nachgewiesener Fehler und implementierter Umfang

`GS_BUFFER_COMPARE` las den Zeichenpuffer stets mit CT32-Adressierung.
Bei FRAME.PSM=CT16/CT16S waren dessen Nichtschwarz-Zahlen daher unzuverlässig.
Die Diagnose nutzt jetzt den tatsächlichen FRAME-PSM. Das ist kein Beleg
für die Ursache des früheren Crash-E3-Schwarzbilds mit CT32.

Im bestehenden gedrosselten Fortschrittssnapshot kommen hinzu:

- `GS_DISPLAY_REGS schema=1`: beide DISPFB/DISPLAY-Sätze, PMODE,
  SMODE2 und BGCOLOR, auch wenn ein Ausgang deaktiviert ist.
- `GS_GRID_HASH schema=1`: separate RGB-Fingerprints für gewählten
  Scanout und aktives Zeichenziel, jeweils mit Gültigkeitsflag.
  Explizite Bytefolge R/G/B, FNV-1a über das 8x8-Raster der Farbprobe.
  Alpha wird ignoriert. Kein Vollbildhash; schwarze Stichproben beweisen
  kein vollständig schwarzes Bild. Gleiche Hashes beweisen keine Bildgleichheit.
  Ungültige Geometrie/PSM oder fehlgeschlagenes GPU-Readback ergeben
  valid=0/hash=0. Beim ersten fehlgeschlagenen Readback wird abgebrochen.

Pro Snapshot maximal 128 zusätzliche Pixelreads; bestehende Ownership-
und Sync-Regeln gelten weiter. Diagnosen können einen notwendigen GX-Resolve
auslösen. Ihr Wii-Zeitaufwand ist noch nicht gemessen. Rendering, IRQs,
Framebufferwahl, JIT und Optimierungsflags bleiben unverändert.

## Reproduktion und tatsächlich ausgeführte Checks

Host: GCC 12.2.0, mit dem vorhandenen `tools/build_host_core.py`:

```sh
python3 tools/build_host_core.py test_scanout_grid_hash test_beta12_20_gx_alpha test_gx_pipeline_r1298 test_beta12_19_framebuffer16 test_beta12_18_scanout test_gs_display_scaling
```

Die ausgegebenen Binaries wurden tatsächlich ausgeführt:

- Grid-Hash: 18 Checks, RGB/Alpha, CT32/24/16/16S, Offset,
  Grenzen, Readbackfehler und Abbruch nach genau einem Syncfehler.
- GX TEST/DATE/AFAIL/FBMSK: 159 Checks, 0 Fehler.
- GX-Pipeline: 324 Fälle, 0 Fehler.
- CT16/16S Framebuffer: 16 Checks, 0 Fehler.
- BETA12.18 Scanout und Display-Scaling: jeweils PASS.

Wii: offizielles Docker-Image
`devkitpro/devkitppc@sha256:4c919aa26151dd43d88ca28c922d1fe2409579a8ba60ef56517baf1abdfb1a48`.
GCC 16.1.0/devkitPPC r50, libogc 3.1.0.

```sh
docker run --rm -v /workspace/Gekko2:/work -w /work \
  devkitpro/devkitppc@sha256:4c919aa26151dd43d88ca28c922d1fe2409579a8ba60ef56517baf1abdfb1a48 \
  bash tools/build_batch1_diag.sh
```

Frisches `build-batch1-diag`, -O1, alle SPEED/DSI/AOT/GX/IDLE-Flags
des BETA12.19-Buildskripts und LIGHTREC_PILOT=1 erhalten.
Fehlendes generiertes lightning.h wird aus dem vorhandenen Template erzeugt.
ELF/DOL-Abgleich PASS; statisch **2.852.172 Bytes MEM1** frei.
DOL SHA-256: `f9d392e7c5038098eceac5e2a21b8a75ddc43f3d6b576af3cebe485e07412994`.
Versionsstring bleibt BETA12.20; dies ist ein Branch-Diagnoseartefakt,
keine neue Releasefreigabe. Zur Identifikation zusätzlich DOL-SHA und
`GS_GRID_HASH schema=1` verwenden. Keine Ausführung auf Wii oder Dolphin.

## Offene Nachweise und nächster Schritt

Crash-Disc und BIOS liegen hier nicht vor. Für die Ursachenentscheidung
zwei vergleichbare Kaltstarts auf echter Wii mit privater eigener Disc:
GX an und Software-GS, gleiche Einstellungen/Spielaktionen. Benötigt werden
SESSION mit Checkpoint sowie zeitlich zusammengehörige Ausschnitte vor/nach
dem Schwarzwerden: PC/EE/IRQ, GS_DISPLAY_REGS, GS_GRID_HASH,
GS_SOURCE_RGB, GS_BUFFER_COMPARE, GS_SCANOUT, GX/GX_WORK,
GS_ALIAS_SYNC, GIF/VU1/XGKICK und FILEIO_TRACE.
Private Logs lokal behalten und nur bereinigte Ausschnitte bereitstellen.

Konstantes schwarzes Raster bei fortschreitenden Draws, geänderter
DISPFB-Auswahl oder Syncfehlern erfordert jeweils andere Folgeuntersuchungen.
Eine genaue EFB/XFB-Bildprüfung, Draw-Reject-Ursachen und ein vollständiger
GS-Paketvergleich sind weiterhin offen. Keine bestätigte Blackscreenbehebung,
Wii-Spielbarkeit oder FPS-Verbesserung. Batches 2–5 sind noch nicht umgesetzt.
