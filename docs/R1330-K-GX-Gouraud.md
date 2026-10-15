# R1330-K – experimenteller GX-Gouraud-Testbuild (Basis: R1330J Boot-Compare)

## Zählersemantik (geklärt aus dem Quelltext)
- `gouraud_tri` (= `g_draw_routes[9]`, "GS_ROUTE") zählt bei `gs_activate_context` jedes eingereichte IIP-Dreieck, **vor** dem Degenerate-Test. Es sagt nichts über rasterisierte Dreiecke.
- `triangles_drawn` zählt nur Dreiecke mit `edge() != 0`, die tatsächlich gezeichnet wurden. `triangles=0` bei `gouraud_tri≈594944` heißt: **kein einziges** davon hatte Fläche (oder IIP lag nicht an) – 594.944 echte Gouraud-Rasterisierungen sind damit nicht belegt.
- IIP wird über `gs_effective_attr_prim()` bestimmt: bei `PRMODECONT.AC=0` kommt IIP aus `PRMODE`, nicht aus `PRIM` (Host-Test hat das gezeigt, `prmodecont_ac` ist nach Reset 0).

## Neue Zähler (`GS_GOURAUD*`-Zeilen im Log)
submitted / degenerate (identical3, two_same, collinear, origin) / offscreen / visible / gx / software, bbox-Pixel (GX vs. SW), Fallback-Gründe (`GS_GOURAUD_SW_REASON`), Zustands-Histogramm der sichtbaren Dreiecke (`GS_GOURAUD_STATE`, Top 8) und die ersten 16 Degenerate-Samples (`GS_GOURAUD_DEGEN_SAMPLE`). Invarianten: submitted = degenerate + offscreen + visible; visible = gx + software; software = Σ Fallback-Gründe.

## GX-Pfad (experimentell, Variante B)
Unveränderte Basis: untexturiert, PSMCT32, kein Z/Blend/ATE/Dither/FBA/FBMASK/Fog, Größe ≤ 640×512.
Änderungen gegenüber J: (1) +0,5-Pixel-Shift der GX-Vertices (GX tastet Pixelmitte, GS-Software Pixelecke), (2) Alpha wird beim Import mit dem Vertex-Alpha geschrieben statt 0 (nur wenn alle drei Vertex-Alphas gleich; sonst Fallback `alpha_varies`), (3) ABE-Gate (Blend → Software, vorher fehlte die Prüfung), (4) Fallback-Gründe pro Dreieck.
`GEKKO2_GOURAUD_EARLY_DEGENERATE`: degenerierte Dreiecke werden vor `gs_activate_context` verworfen (spart CPU; Ergebnis identisch, da sie nichts zeichnen).

## Auf der CPU verbleibt (nicht in GX)
Texturierte Gouraud-Dreiecke, Z-Test/-Write, Alpha-Blend, ATE, Dither/FBA, FBMASK, Fog, Nicht-CT32-Formate, variierendes Alpha, große Dreiecke – alle in Software (`gx_inactive`/`texture`/`depth`/`blend`/… im Log). Texturlinking und Fog über GX sind **nicht** implementiert – ohne Messdaten, welche Zustände die sichtbaren Dreiecke tatsächlich haben, wäre das geraten.

## Builds
- A `Gekko2-R1330K-A-Counters` (`-DGEKKO2_GOURAUD_GX_DISABLE`): nur Zähler, GX-Gouraud aus → Vergleichsbasis zu J.
- B `Gekko2-R1330K-B-GX-Gouraud` (`-DGEKKO2_GOURAUD_EARLY_DEGENERATE`): experimenteller GX-Pfad.
- Optional `-DGEKKO2_GOURAUD_GX_MIN_PIXELS=N`: kleine Dreiecke bleiben in Software.
Beide ohne `PCSX2WII_FAST` (Boot-Compare-Modus). Script: `tools/build_r1330k.sh`.

## Grenzen
Kompilieren ≠ korrekte Grafik oder höhere FPS. Weder Bildkorrektheit noch Geschwindigkeit des GX-Pfads wurden auf echter Wii/Dolphin geprüft. Ob Gouraud überhaupt der Engpass ist, ist offen (Readbacks der texturierten Sprites mit Blend/Z dominieren laut Log).
