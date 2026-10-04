# R1296 — one experimental GX option

2026-10-04. This release keeps the R1295 CPU/JIT/GS optimizations and changes launcher control only.

Settings displays `GX (EXPERIMENTAL): ON/OFF`. LEFT and RIGHT both toggle one shared option. ON requests GX presentation and supported primitive drawing together; OFF selects software. First-image, initialization and supported GS state gates remain active. This is not a claim that textured/blended/complex GS operations have been ported to GX.

Logs are now `sd:/pcsx2/R1296-software.log` or `sd:/pcsx2/R1296-gx-render.log`. The obsolete output-only selection is removed. UI header revision and preview tools are updated. Both DOL/ELF engines are supplied. Copy the JIT DOL to `sd:/apps/pcsx2-wii/boot.dol`.

Validation: build both engine pairs, execute linked PPC boot policies and GX submission/fallback checks, and visually inspect the settings render. No physical R1296 Wii or Dolphin run has been performed here. R1295 passed 193 native tests and twelve linked PPC jobs; those are prior core validation, not a fresh full-suite claim for this UI change.

Dolphin emulates Wii graphics, so this GX command path can also be tested there. Start with native internal resolution (1x), no MSAA, `Store EFB Copies to Texture Only` OFF and `Store XFB Copies to Texture Only` OFF. These are conservative project test settings because the CPU consumes readback/copy data, not a measured best-performance preset. If needed disable deferred EFB RAM copies during diagnosis. Dolphin timings are not physical Wii FPS evidence.

References: https://dolphin-emu.org/blog/2017/11/19/hybridxfb/ and https://dolphin-emu.org/docs/guides/performance-guide/ .

The cumulative Claude patch/diff applies to the original full-debug archive. The checkpoint contains binaries, patch/diff, R1295 evidence plus R1296 UI checks, and no BIOS/game/private runtime state.
