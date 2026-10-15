# Gekko2 R1376 A/B — sparse GX framebuffer initialization

R1375 B still froze on the physical Wii. Its log confirms partial readbacks saved 4.40 GB, but render-target initialization uploaded 4.87 GB. The exact freeze instruction is unknown. This revision targets initialization and preservation of framebuffer contents; it is not a confirmed crash fix.

## Changes

A new per-target 4x4 tile map records which EFB tiles are initialized. Before a draw, only missing tiles inside its coverage rectangle are uploaded from CPU VRAM. Already initialized tiles are never reseeded from stale CPU memory. Each new tile initializes its exact CPU alpha shadow. Upload cache flushes cover distinct 64-byte GX tile ranges. Target changes drain previous allocation consumers and reset the map.

Sparse resident targets resolve their dirty rectangle before normal scanout. The full-target resident presentation shortcut is bypassed because uninitialized EFB pixels must never appear in the displayed framebuffer. R1375 partial readbacks and seed-only release remain.

A: resident blend disabled, sparse initialization enabled.
B: bounded additive blend, sparse initialization, resident completion fence enabled. The fence can reduce speed. Neither variant enables frequent SD dump writes.

Existing JIT budget ranges, DSI fastmem and the Lightrec pilot remain. No new ARM worker or ARM EE/VU offload.

New log line: GX_SPARSE_SEED uploaded_bytes, tile_runs, reused_tiles. uploaded_bytes counts actual seed bytes, reused_tiles counts skipped tile initializations. Neither is a Wii FPS prediction.

## Framebuffer and borders

CPU and GX CT32 framebuffer addressing were compared. A new native regression checks every pixel at BIOS framebuffer bases 0, 163840 and 491520 (word units), GX RGBA8 tile layout, complete VRAM import, edges, nonzero origins and odd cropped dimensions/padding. All passed.

The logged SMODE2=3 / DISPLAY=0x1ff9ff01848290 decodes to 640x256. The INT+FFMD half-height behavior is also present in PCSX2's GSState.cpp. It was retained. No arbitrary crop/stretch or proven fix for the user's black borders or malformed quadrilaterals is claimed. Those symptoms require checking actual EFB output; the tests below do not emulate GX pixels.
Reference: https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS/GSState.cpp

## Validation

43 native regression programs passed. Linked PPC with mocked GX calls verified a smaller first seed upload, no reseeding on repeated draws, a disjoint later seed preserving existing alpha, partial resolve and completion fences. A's resident blend rejection passed. Both DOL files match ELF load sections and fit MEM1. GCC 16.1.0 and libogc 3.1.0.

No physical Wii or Dolphin runtime test here. Freeze stability, EFB rasterization and FPS remain unverified.

## Install/test

Use A_Control/boot.dol first, then B_Sparse_Additive/boot.dol with identical BIOS/options and a fresh Cold Boot. Save each log separately: the session filename uses R1376 while BUILD distinguishes A/B. Report whether black borders/quad artifacts differ between A and B, and save the log before another session.
