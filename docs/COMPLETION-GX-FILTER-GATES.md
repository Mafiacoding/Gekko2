# C02b: Native GX-Filterzulassung

Die native Texturkonfiguration transportiert bisher genau einen Filterwert.
Sie kann gemischte GS-Min/Mag-Modi und per-Fragment-Mip-Auswahl nicht einfach
wie gemeinsame nearest/nearest oder linear/linear behandeln. Solche Draws
werden nun ausdrücklich aus den entsprechenden nativen Texturpfaden abgelehnt;
Software-/Source-Shading bleiben verfügbar. Source-Shading verwendet ebenfalls
das interpolierte Q zur Filterauswahl statt des zuletzt geschriebenen RGBAQ.Q.

Die Zulassungsregression prüft alle zwölf Min/Mag-Kombinationen (sechs gültige
MMIN und beide MMAG); die beiden vorhandenen gleichmäßigen Basisfilter bleiben
nativ zugelassen. Bisherige common-GIF-Zulassung, 19584 Shading-Pixel und
11701 Q/LOD-/MTBA-Fälle bestehen. Keine Behauptung vollständiger GPU-Mipmaps.
