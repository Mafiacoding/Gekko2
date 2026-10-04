# R1289 — deferred GPU VRAM synchronization

## Implemented

Shared VRAM now tracks pending GPU ownership with one resolver/context. All public color, depth, indexed/nibble, span, fill and raw-memory accessors resolve GPU work before CPU access. Binding a new resolver drains the previous owner; missing/failed resolution retains ownership and CPU reads/writes fail closed. Reentrant access during resolution is rejected. Initialization drains before clearing. Checkpoint save/load drain before their state operations. Raw pointers must not be retained across GPU work. This is a single-threaded, conservative full-VRAM barrier, not per-page parallel scheduling.

GX has a bounded EFB capture API for already-rendered opaque PSMCT32 RGB rectangles at EFB origin 0,0. Width/height are multiples of four, at most 640x512 and within the last configured EFB dimensions. Destination bounds are checked before any copy. A known GS alpha byte is supplied separately; Z, varying alpha and other GS formats are unsupported by this capture API.

The aligned readback buffer is flushed before GX_CopyTex. The first subsequent CPU access waits with GX_DrawDone, invalidates the CPU copy buffer and imports GX RGBA8 tiles into swizzled PS2 little-endian VRAM. Import validates the entire target before writing and does not use the display broadcast clamp. GX presentation and shutdown drain pending work before using/replacing EFB. The readback buffer is retained until process exit.

The capture API queues a copy, not a PS2 draw. The software renderer does not yet submit primitives to GX. Consequently no GPU rendering speedup is claimed. CPU accessors add a pending-state check. Isolated PPC fill-triangle workload rises from 953916 to 1100220 instructions (+15.34%); fast sprite fill from 165581 to 167849 (+1.37%). These are conservative correctness costs before GPU primitive offload, not Wii timing/FPS. Hardware GPU waits have no timeout here. The full JIT and GS-to-GX primitive translation remain open.

## Verification

186/186 native tests pass. The new sync test additionally covers all storage aliases, write/fill barriers, row reads, raw access, known-alpha tiled import, source/target bounds, failure/retry, reentrant access and reset. Actual linked PPC ELF tests exercise deferred resolver read/write ordering, failure ownership and retry, a 32-pixel page-crossing import and unchanged full VRAM after invalid imports in both builds.

Capture orchestration is executed in both ELF variants with mocked GX/cache services: flush -> queued copy -> deferred wait -> invalidation -> import. This is not proof of physical GPU/cache behavior. Inherited GS bounds, 66 complete VRAM render signatures, GX packing and IOP JIT checks pass. All 66 draw images per build remain identical to the R1285 verified baseline.

The GS bounds fixture was updated because R1289 places ownership fields immediately before VRAM: its prefix canary must preserve zero ownership flags/pointers. The older fixture overwrote live state, causing an artificial invalid callback. A synthetic PPC callback repair also invalidates Unicorn's translated-code cache before retry. These were test-fixture corrections, not suppressed production failures.

No new real-Wii cold boot, FPS improvement, stable OSDSYS or GX primitive result is claimed. GX presentation remains opt-in through Settings RIGHT.

## Install

PCSX2-Wii-R1289-Menu-JIT.dol -> sd:/apps/pcsx2-wii/boot.dol. ELF is for Dolphin/debugging; interpreter variants are included. Preserve BIOS/config/discs. Logs: sd:/pcsx2/R1289-boot.log. No BIOS/game image is included.

Cumulative Claude.patch/.diff apply to the original uploaded pcsx2-wii-full-debug(1).zip source and are byte-verified. The checkpoint includes binaries, patches, licenses and verification. GitHub publication remains pending.
