# Shaka Player GUI (Windows)

Electron-App mit eingebettetem Google Shaka Player (DASH, HLS, MP4/WebM, …).

- Datei öffnen (Strg+O), Drag & Drop oder URL einfügen
- ClearKey: Zeilen `KEY:ID` (Hex) oder `ID:KEY` (Umschalter)
- Eigene Header: `Name: Wert` pro Zeile oder JSON; wirkt auf alle HTTP-Anfragen
- Kein Widevine/PlayReady-CDM in Electron

Bauen (Windows oder Linux): `npm install && npm run dist` → `dist/Shaka Player GUI 1.0.0.exe`
Starten zum Entwickeln: `npm start`
