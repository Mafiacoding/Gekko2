# BETA12.27 – GX Risk Mode / Fast Triangles

Basis: `dea99ddabc79836440f4b812f06bca3fe47617d0` (BETA12.26).
Branch: `codex/beta12-27-gx-risk-mode`.

## Benutzung

Im Wii-Menü **Settings → CPU/GX Optimizations** den Eintrag
**GX RISK MODE – FAST TRIANGLES** auswählen und mit **A / Links / Rechts**
ON/OFF schalten. Vom ersten CPU/GX-Eintrag führt **UP** direkt zu diesem
zusätzlichen Eintrag. Die Bedienung benötigt keine zusätzliche Schultertaste
und funktioniert über die vorhandene Menü-Tastenzuordnung.

Standard: **OFF**. Die Einstellung wird unter `sd:/pcsx2/gx-risk.cfg` gespeichert:

```
GEKKO2_GX_RISK_MODE 0
```

bzw. `GEKKO2_GX_RISK_MODE 1`. Sie wird erst beim nächsten **Cold Boot** aktiv.
START/Resume übernimmt eine Änderung nicht in eine pausierte Sitzung.
Chain-Residenz und experimentelle PSM-Konvertierung behalten ihre eigenen
Einstellungen; dieser Schalter aktiviert sie nicht mit.

Bei Grafikfehlern auf OFF stellen, speichern und Cold Boot ausführen.
Der Launcher wendet den Risikomodus nicht bereits beim Laden des Menüs an.
Die SD-Pakete überschreiben keine bestehenden Konfigurationsdateien.

## Was ON tatsächlich ändert

OFF verwendet den bisherigen BETA12.26-Pfad mit getrennten GX-Zeilenabschnitten.
ON darf ein geeignetes Dreieck einmal komplett an GX übergeben. Dafür müssen
alle GS-bedeckten Pixel RGB schreiben dürfen und denselben PABE-Blend-Gate
besitzen. Scanmasken, unterschiedliche Alpha-/DATE-/Z-Farbfreigaben und
nicht unterstützte Zustände bleiben beim bisherigen Pfad.

Das spart bei geeigneten großen Dreiecken GX-Befehle und Vertexübermittlungen.
Der zusätzliche Zulassungstest benötigt CPU-Zeit; ein Netto-FPS-Gewinn ist
nicht garantiert. Die CPU berechnet weiterhin exakte GS-Pixelberechtigungen
und kanonische Alpha-/Z-Schreibwerte. Das GPU-Scissor benutzt die ursprüngliche
BBox, nicht die auf vier Pixel aufgefüllte Uploadgröße.

**Bewusstes Darstellungsrisiko:** Die RGB-Kantenabdeckung wird im schnellen
Pfad direkt dem GX-Rasterizer überlassen. Abweichungen zum GS können zusätzliche
oder fehlende Kantenpixel erzeugen. Auch die bisherige experimentelle
Textur-/Gouraud-/Blendpräzision bleibt. Der Modus ist kein vollständiger
GS-Hardware-Ersatz und schaltet keine offenen Mipmap-/Feedback-/Maskenpfade frei.

Speicher-/Aliasprüfungen, GX-Fences, Timeout-/Fehlerbehandlung und Cold-Boot-
Ownership bleiben bestehen. Die Einstellung verändert keine IRQ-Quittierung,
Gastzyklen, JIT-Save-/Restore-Regeln oder Spielassets.

## Kompilierung und Diagnose

`bash tools/build_beta12_27_gx.sh` baut das Standardprofil mit
`GEKKO2_GX_RISK_MODE`, dem gemeinsamen GX-Shader und den erweiterten Dreiecken.
Verfügbarkeit setzt alle vier Flags voraus: `GEKKO2_GX_RISK_MODE`,
`GEKKO2_GX_GENERIC_SHADER`, `GEKKO2_GX_EXTENDED_TRIANGLES`,
`GEKKO2_GX_NATIVE_BIOS_TRI`. Andere Builds zeigen UNAVAILABLE; eine gespeicherte
1 kann den fehlenden Backend-Pfad nicht aktivieren. Interpreterbetrieb ist
keine zusätzliche Einschränkung für den GX-Schalter.

Die bisher volle 32-Bit-CPU/GX-Optionsmaske bleibt unverändert. Der neue Eintrag
ist eine separate experimentelle Option mit eigenem Schlüssel und bleibt
auch bei fehlenden/ungültigen Dateien OFF. Bestehende Chain-/PSM-Dateien sind
weiter kompatibel.

SDK: devkitPPC r50 / GCC 16.1.0 / libogc 3.1.0,
`devkitpro/devkitppc@sha256:4c919aa26151dd43d88ca28c922d1fe2409579a8ba60ef56517baf1abdfb1a48`.
SPEED/DSI/AOT, volle Ausgabegröße und die bisherigen Standardoptionen bleiben.

Im Log:

```
GX_RISK_MODE available=... requested=... enabled=... candidates=... fast_draws=... fallback=... vertices_saved=... apply=cold-boot coverage=experimental-GX-edges
```

`requested` ist die Menüwahl, `enabled` der Wert der laufenden Sitzung.
`candidates` und `fallback` zählen nur bereits zugelassene native Dreiecke,
die den Risikoplaner erreichen; sie sind keine Gesamtzahl aller Software-Rückfälle.
`vertices_saved` vergleicht die schnellen Draws mit deren bisherigem
Zeilenpfad; es ist kein FPS-Messwert. Bootlogs nennen zusätzlich die
Verfügbarkeit und die noch nicht angewendete Auswahl.

## Prüfung

- `tools/test_beta12_27_host.py --output-json <Datei>`: 55 Host-Suites,
  einschließlich aller bisherigen BETA12.26-Regressionen.
- Neue Risikoprüfung: 131.072 Farb-/PABE-Masken, Scanmasken, Grenzen,
  transaktionale Plan-Ausgaben, OFF-Standard, Speicherung, striktes Parsen,
  unabhängige Optionen und Cold-Boot-Isolation.
- `tools/test_gx_risk_profiles.py`: zusätzlich vier Host-Buildprofile: unkompiliert, Flag ohne Backend,
  vollständiger Risikobuild und Risikobuild mit Chain-Unterstützung.
- `verify_gx_risk_beta12_27.py`: tatsächlicher gelinkter PPC-Code. Im
  synthetischen geeigneten Dreieck OFF=16 GX-Übermittlungen, ON=1; Alpha-/Z-
  Metadaten unverändert. Geprüft sind verzögerte Aktivierung/Deaktivierung,
  originale 17×17-BBox statt 20×20-Upload-Padding, Rückfälle bei PABE,
  Alpha, DATE, Z und SCANMSK sowie Ablehnung von FBMSK und deaktiviertem GX.
- Die enthaltenen BETA12.25-/BETA12.26-PPC-Prüfungen laufen zuerst mit
  standardmäßig deaktiviertem Risikomodus. Bestehende Fences, Cold Boot,
  Ownership und Formate werden zusätzlich geprüft.
- Wii-DOL-/ELF-Sektionen und statisches MEM1 werden geprüft.

GX-Aufrufe sind in den PPC-Prüfungen simuliert. Echte Kantenpixel, Wii-FPS,
Tekken-/Crash-Spielbarkeit oder Kachelkorrekturen sind nicht nachgewiesen.
Es werden ausschließlich synthetische Testdaten veröffentlicht.
