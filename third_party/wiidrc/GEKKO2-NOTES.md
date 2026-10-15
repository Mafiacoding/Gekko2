# libwiidrc in Gekko2

Upstream: https://github.com/FIX94/libwiidrc

Copyright 2017 FIX94, MIT; LICENSE retained. Source retrieved 2026-10-10.

Modifications: wiidrc.c includes the colocated wiidrc.h; added WiiDRC_Reset
to discard cached firmware pointers/status before IOS lifecycle changes.
Original button/state decoding otherwise retained.

This is input-only support for patched Wii VC firmware. It does not install
firmware, unlock clocks, render to the GamePad or implement native Wii U APIs.
