# BETA12.31c verified downloads

Wii build code `75cbe6a`; packaged source/receiver `c18ef1a`. Default network OFF.
Combined SD ZIP includes Wii app and Windows-Logger 1.1. Update both.
No BIOS, games, owner configuration, logs or network addresses included.

- [Gekko2-BETA12.31c-SD.zip](https://raw.githubusercontent.com/Mafiacoding/Gekko2/6171afd8c02e7e0a87269e661a2e9fc767fe3792/downloads/Gekko2-BETA12.31c-SD.zip) — 940172 bytes; SHA-256 `0704b56535493c545fba62d69bf7f5dd909b6dc37b5393a628838d45b334bc80`.
- [Gekko2-Windows-Logger-1.1.zip](https://raw.githubusercontent.com/Mafiacoding/Gekko2/6171afd8c02e7e0a87269e661a2e9fc767fe3792/downloads/Gekko2-Windows-Logger-1.1.zip) — 7690 bytes; SHA-256 `319a0927ff8d8c373ba8cc7e8cab7d55baadd4af768262d018b99335dafbb8ab`.
- [Gekko2-BETA12.31c-review.zip](https://raw.githubusercontent.com/Mafiacoding/Gekko2/6171afd8c02e7e0a87269e661a2e9fc767fe3792/downloads/Gekko2-BETA12.31c-review.zip) — 13159372 bytes; SHA-256 `f5a710e44fdb9ebab92bdba2591c4061d4eca33a32e2b338ca2fa0ce7e48af41`.

All final immutable URLs refetched with HTTP 200, exact byte/SHA equality and
ZIP CRC checks. Host protocol/menu/UDP/Windows-style-reset regressions, four
existing CPU/menu/config suites, real EE/VU/GX profile work, 33 wire/FIFO PPC
checks and 49 API/worker PPC checks passed. SDK network/thread/IRQ/FPU/timebase
modeled for worker checks. Fresh Wii DOL/ELF/MEM1 gates passed.
Actual Wii/Windows 1.1 receipt remains unverified; use the acknowledged menu
probe without a BIOS boot. No established old-failure cause or GX/BIOS/FPS fix.

[German setup](../tools/windows-telemetry/README-DE.md) · [report](BETA12.31c-NETWORK-SETUP.md).
