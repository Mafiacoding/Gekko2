# Gekko2 R1367 – EE-Verknüpfung und GX-Texturcache

Basis R1366. devkitPPC r50 / GCC 16.1.0, libogc 3.1.0. Stand 10. Oktober 2026.

## Implementiert

**EE:** Ein erlernter Nachfolger wird jetzt vor der allgemeinen Cache-Suche geprüft. Eigentümerindex, PC, Funktionsadresse, Serial, Budget sowie RAM-/Mapping-Generation müssen passen. Andernfalls greift die bisherige Suche nach geeigneten Varianten. Aktive Ketten halten ihre Owner fest; die Prüfung des tatsächlich vorliegenden ersten Opcodes und die bisherigen Instruktions-/Speichergrenzen bleiben erhalten. Dies beschleunigt die abgesicherte Nachfolgersuche; es ist kein unbedingter PPC-Sprung ohne Prüfungen.

**GX:** Ein bereits gepackter und veröffentlichter Texturpuffer wird bei einem gültigen Source-Cache-Treffer nicht nochmals per DCFlushRange aus dem CPU-Cache zurückgeschrieben. Ein Pending-Zustand stellt sicher, dass neu gepackte Daten auch nach einem abgebrochenen Zeichenversuch vor ihrem ersten erfolgreichen Verbraucher geflusht werden. Änderung des Schlüssels, des Ursprungs oder eine ungültige Quelle erzwingt weiterhin erneutes Packen und Veröffentlichung. Das gilt für residenten Texturpfad, den bisherigen Texturpfad und affine Texturdreiecke.

Frischer Scanout ist ein anderer Produzent: er packt neue Bilddaten und behält seinen verpflichtenden Flush. GX-Texturcache-Invalidierung, EFB-Kopien, Tiefen-/Blendzustände und notwendige CPU-Readbacks bleiben erhalten. Neue Logangabe: `GX_SOURCE_CACHE avoided_cpu_flush_bytes` zeigt den Umfang eingesparter CPU-Cache-Flush-Anfragen; keine tatsächliche Busbandbreitenmessung.

R1366 IOP-/EE-Optimierungen und vorherige Spiele-/Sound-/Controller-Betas bleiben enthalten. Keine neue ARM-/IOS-Funktion und keine nachgewiesene Reparatur der VU1-Geometrie.

## Messung und Tests

Warme EE-Sprungschleife mit zwei wiederkehrenden Ownern, 100 Aufrufe à 32 Gastinstruktionen nach Warmup, tatsächlicher verlinkter PPC-Code unter Unicorn:

| | R1366 | R1367 |
|---|---:|---:|
| Hostinstruktionen im Messabschnitt | 1,841,200 | 1,747,849 |
| Gastinstruktionen einschließlich Warmup | 3328 | 3328 |

**5.07 % weniger Hostinstruktionen**, identischer vollständiger EE-Zustands-Hash. Kein Wii-FPS-/Bootzeitversprechen; kein pauschaler Gewinn für jedes Spiel.

Bestanden:

- 38 native Regressionstests, einschließlich GS-Pipeline, Surface-Import und Schwarz-/Scanout-Konvertierung zusätzlich zu den bisherigen IOP-/VU-/IPU-/Sound-/DMA-/Controller-/Cache-Tests.
- 126 PPC-Ketten-Budgets sowie geänderter Nachfolgecode, Timergrenzen, erhaltene PPC-Register und Allokationsfehler.
- 12 EE-Präfixvergleiche mit vollständigem EE-/Timer-/IRQ-/Display-Zustand und anschließendem RAM-Store, einschließlich vollem Cache.
- 26 EE/IOP/Timer-Schedulervergleiche und sechs gemeinsame Mailboxfälle.
- GX-PPC-Test: sechs Pack-/Cachefälle, gleiche Texturbytes, Veröffentlichung nach abgebrochenem Draw, Quellen-/Ursprungsänderung und frischer Scanout. GPU-/Cachemaintenance werden modelliert; kein physischer GX-Rastertest.
- DOL-Sektionen bytegleich zur finalen ELF; Wii-Startmarkierungen vorhanden.

## Internet- und Quellcodevergleich: mobile GS-Ansätze

Play! ist hier die überprüfbare mobile Referenz. Das Android-Build verknüpft seine GS-Backends; die eigentliche GS-Logik sitzt nicht allein in der Android-Oberfläche. Die folgenden Befunde stammen aus den heruntergeladenen Originaldateien, nicht aus fremden Leistungsversprechen.

| Gelesener Code | Beobachtung | Konsequenz für Gekko2 |
|---|---|---|
| Play! OpenGL `SetRenderingContext` | Bei identischen relevanten GS-Registern und gültigem Textur-/Framebufferzustand frühzeitige Rückkehr; Flush vor notwendigen Zustandsänderungen | Zustandsarbeit nach tatsächlichen Änderungen bündeln; eine GX-Zustandscache-Erweiterung muss auch EFB-/Präsentationspfade invalidieren |
| Play! OpenGL `CommitFramebufferDirtyPages` | Bearbeitet veränderte Framebufferseiten über Dirty-Rechtecke | CPU/GPU-Eigentümerschaft und betroffene Bereiche verfolgen, statt jeden Zugriff mit vollständigem VRAM-Import zu beantworten |
| Play! Vulkan `CDrawMobile::CreateRenderPass` | Unswizzle-/Zeichen-/Swizzle-Pässe mit lokalen Attachments, Input-Attachments und expliziten Abhängigkeiten | Lokalität und weniger Speicher-Rundreisen sind übertragbare Ziele; die Vulkan-Shader-/Subpass-Implementierung ist kein direkter GX-Port |
| nullDC4Wii `gxRend.cpp` | Erfasst u. a. letzten Textur-, TEV- und Blendzustand und setzt sie bei Sonderpfaden zurück; eigener GX-Texturpfad | Ein begrenzter GX-Zustandscache ist denkbar; Dreamcast-Polygon-/VRAM-Semantik darf nicht als GS-Semantik übernommen werden |

R1367 setzt einen kleinen, überprüfbaren Teil dieses Prinzips um: unveränderte Daten und bekannte Nachfolger nicht erneut bearbeiten. Ein vollständiger GPU-Framebuffer-/TEV-Zustandscache oder ein Play!-Renderer-Port wurde nicht implementiert. Androids Backendstrategie dient als Referenz; ARM-/Vulkan-Code ist kein Ersatz für Broadway-PPC und GX.

PCSX2 bleibt als offen prüfbare PS2-Referenz für GS-/Cache-/Synchronisationssemantik nützlich. Für AetherSX2 liegt hier kein verifizierter Originalquellcode vor. Das gefundene Drittanbieter-Repository wurde deshalb nicht als Beleg seiner internen Implementierung verwendet.

## Entfällt Synchronisation bei anderen Emulatoren?

Nein. PCSX2 besitzt `MTGS::WaitGS`, eine GS-Befehlswarteschlange und gezielte Wartepfade. Im gelesenen Play!-Vulkan-Code stehen Speicherbarrieren, Renderpass-Abschlüsse und `vkQueueWaitIdle` in bestimmten Lebenszyklus-/Transferpfaden. Das bedeutet nicht, dass jeder Draw die ganze GPU abwartet.

EE↔IOP-Zeit-/Datenübergaben und CPU↔GPU-Datenveröffentlichung sind verschiedene Synchronisationen. Die bestehenden SIF-Barrieren bleiben in R1367 erhalten. Selbst eine identische erneut geschriebene Registerzahl kann eine Veröffentlichung anderer RAM-Daten markieren. Diese Barriere darf deshalb nicht allein aufgrund unveränderten Registerwerts wegfallen. Vor einer weiteren Reduktion brauchen wir die bestehenden `SYNC_REASONS` und den zugehörigen Gastpfad aus einem tatsächlichen neuen Wii-Lauf.

## Späterer Wii-Test

R1366 und R1367 mit identischen Einstellungen per Cold Boot bis zur gleichen BIOS-/Tekken-Phase laufen lassen. Die EE-Nachfolgersuche braucht die vorhandenen EE-JIT-/Native-Block-/Link-Pfade. GX-Einsparung benötigt tatsächliche akzeptierte GX-Texturarbeit und die vorhandene Textur-Wiederverwendung. Wenn fast alles Software bleibt oder die Geometrie degeneriert ist, bringt diese GX-Änderung dort entsprechend wenig.

Log mit Buildkennung R1367, EE/s, Guest-VBlank/FPS, direkten Link-/Cachezählungen, `SYNC_REASONS`, `GX_SOURCE_CACHE` und Profilanteilen sichern. Bis dahin keine bestätigte Wii-Beschleunigung, kein neuer Tekken-Durchlauf und keine vollständige Spielekompatibilität.

## Primärquellen

- [Play! OpenGL](https://github.com/jpd002/Play-/blob/master/Source/gs/GSH_OpenGL/GSH_OpenGL.cpp), gelesene Datei SHA256 `d1f64287d2b2ce1e6b1fdc580913c0b95d768682c30d29836bc7f8fba581f182`.
- [Play! Vulkan](https://github.com/jpd002/Play-/blob/master/Source/gs/GSH_Vulkan/GSH_Vulkan.cpp), gelesene Datei SHA256 `9b57f8b523207a7ea70d8b7020f16e6fc662c4f3eef56b717c0b1da168cb1f3e`.
- [Play! Vulkan Mobile](https://github.com/jpd002/Play-/blob/master/Source/gs/GSH_Vulkan/GSH_VulkanDrawMobile.cpp), gelesene Datei SHA256 `24d0bda3ec3493bc3614ccf8adf910c27b3d74d8d197290131064be03b6911a8`.
- [nullDC4Wii GX](https://github.com/BenoitAdam/nullDC4Wii/blob/main/plugs/drkPvr/gxRend.cpp), gelesene Datei SHA256 `343f4b86fb92a61cea11968ff9be01b2ea0c97aace250d3029a0471a3a5c3172`.
- [Play! Android Build](https://github.com/jpd002/Play-/blob/master/Source/ui_android/CMakeLists.txt).
- [PCSX2 MTGS](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/MTGS.cpp).
- [PCSX2 GS Texture Cache](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS/Renderers/HW/GSTextureCache.cpp).
- [libogc GX API](https://github.com/devkitPro/libogc/tree/master/libogc).

Externe Renderer wurden untersucht, nicht als komplette Module übernommen. Quellcode- und Lizenzhinweise der bestehenden Drittanbieterbibliotheken bleiben im Projekt erhalten.
