# Gekko2 R1362 Beta

Die neue Änderung reduziert unnötige Sucharbeit im nativen Code-Cache. Der VF00-Fix aus R1361 bleibt enthalten.

## Implementiert

- Kleine freie Codebereiche bis 1 KiB erhalten exakte, 32-Byte-aligned Größenklassen; größere Bereiche vier Klassen pro Zweierpotenz.
- Drei 32-Bit-Bitmap-Wörter statt einer groben 18-Klassen-Suche. Keine 64-Bit-Bitmap-Arithmetik auf Broadway.
- Codearena weiterhin 6 MiB in MEM1. Veröffentlichte Blöcke behalten ihre Adressen; Freigabe, Zusammenführen, Admission-Reuse und DSI-Patching bleiben erhalten.
- Rund 320 Byte zusätzliche Verwaltungsdaten. Kein Global-Flush und keine Änderung der Gast-Zeitrechnung.
- Logmarkierung: `lookup=exact32-to1KiB-4way` in CODE_ARENA_LAYOUT.

## Prüfung

Gezielter Fragmentierungstest: 1.000 nicht passende freie Löcher vor einer passenden Fläche. R1361 prüft 1.001 Bereiche, R1362 einen. Das ist ein Allocator-Mikrotest, keine 1.001-fache Emulatorbeschleunigung.

21 portable Regressionstests bestanden, einschließlich 100.000 zufälliger Cache-Allokationen/Freigaben/Trims, Admission-Reuse, vollständiger Zusammenführung und VU0-Kaltstart.

362 synthetische VF00-Coprozessor-Encodings stimmen zwischen PPC-JIT und Interpreter überein.

PowerPC-Präfixprüfung: vollständiger EE-, Timer-, IRQ-, Display- und RAM-Zustand sowie Owner-Freigabe bei voller Arena bestanden. DOL-Sektionen stimmen mit ELF überein; Wii-Startup-Signaturen vorhanden.

Build: devkitPPC GCC 16.1.0, libogc 3.1.0; ARM-Beta, DSI-Fastmem und bestehender Lightrec-IOP-Pilot enthalten. Lightrec ersetzt keinen R5900-EE-Kern.

Ein realer Wii-/Dolphin-Lauf und ein FPS-Gewinn sind noch nicht bestätigt. Der bekannte VF00-Fix ist keine Bestätigung, dass sämtliche Tekken-Geometrieprobleme verschwunden sind.

## Wii-Test

Mit denselben Optionen wie R1361 kalt starten. CODE_ARENA eingeschaltet lassen. BIOS bis zum ersten Bild laufen lassen und anschließend Tekken testen. Vollständiges Log senden; Bootzeit, EE_per_s, guest_vblank_mHz und Cache-scans pro Allokation vergleichen. Während des Vergleichslaufs keine Optionen wechseln.
