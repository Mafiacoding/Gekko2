# R1276: SD image browser, launcher typography and EE dispatch

## Delivered behavior

The native launcher now has SELECT DISC. It opens sd:/pcsx2/games/ and falls back to sd:/ if that folder is absent. Browse folders with A; select ISO/BIN files case-insensitively, including filenames with spaces. B goes up a folder (back at root), L/R moves eight entries, START returns to the launcher. Both directories and image files are sorted with directories first. The list grows dynamically, with eight visible rows and explicit allocation/filename-limit indication instead of silent list overflow. Paths are bounded to 511 bytes.

After selecting a file, START DISC boots a new session with that exact path through both CDVD and legacy CD-ROM mounts. It no longer requires renaming to game.bin/game.iso. START DISC opens the browser when no image has been selected. A failed mount reports the selected filename and returns to the launcher. Selection lasts for this emulator process; it is not written to a persistent preference file. Selecting a different image while paused does not swap a disc into the running guest: START DISC starts the new boot, while START resumes the previous session. BIOS / OSDSYS remains diskless.

The native launcher now uses coverage-antialiased DejaVu Sans Mono glyphs rasterized separately at 8x14 and 24x42 cells. The large title is no longer a 3x enlarged tiny bitmap. XFB blending respects two-pixel chroma sharing and preserves neighboring luma. DejaVu license is bundled, and tools/generate_frontend_font_r1276.py can reproduce the compiled font using the recorded font hash. Font rendering occurs only when the launcher redraws; it is not added to each emulated frame.

## Resolution diagnosis

The authenticated SCPH-50004 OSDSYS checkpoint decodes as a 640x256 source field, SMODE2=3, with the existing presentation renderer scaling it to the selected Wii framebuffer. The inspected host presentation is 640x480. R1274's vertical field interpolation remains in use. The BIOS's intrinsic lower-resolution field and texture/font sampling can still look soft or pixelated. No artificial 640x480/HD GS render resolution, weave deinterlacing or new GS texture filtering is claimed here.

Every boot progress log now includes a VIDEO line with active circuit, source dimensions/origin, output dimensions, SMODE2 and DISPLAY. This permits checking the real preferred Wii mode rather than assuming 640x480 (PAL and progressive preferences can differ).

Images: Launcher Before/After are host renders of the same final layout with the old bitmap path versus the new coverage path. PAL preview uses 640x576. Browser preview uses illustrative filenames; it is not evidence of Tekken gameplay. OSDSYS image is a real guest checkpoint rendered on the host. None is a physical Wii/Dolphin screenshot.

## Measured EE change

Added direct CPU-loop execution for SRL/SRA/SLLV/SRLV/SRAV, AND/OR/XOR/NOR, SLT/SLTU, SUBU, DADDU/DSUBU after measuring their single-op JIT overhead. Native translators remain available for direct API calls and future EE blocks. R1275's cheaper paths are retained. A fast SLL/ADDU check precedes the additional byte classifier: an initial classifier draft regressed those already-cheap cases and was replaced before release.

Actual Wii ELF PPC instruction counts for eight operations including ordinary retirement, Count and PC advancement, zero-register validation. Cold sample plus three warm samples; median shown because periodic timers add work in individual samples. Allocation/cache maintenance are mocked. This is not Wii cycles, a BIOS workload profile or a claim of FPS improvement.

| Instruction | R1275 cold | R1276 cold | R1275 warm median | R1276 warm median | Warm reduction |
|---|---:|---:|---:|---:|---:|
| ADDIU | 5017 | 5017 | 5017 | 5017 | 0.00% |
| LUI | 5129 | 5129 | 5129 | 5129 | 0.00% |
| ORI | 5145 | 5145 | 5145 | 5145 | 0.00% |
| ANDI | 5145 | 5145 | 5145 | 5145 | 0.00% |
| XORI | 5145 | 5145 | 5145 | 5145 | 0.00% |
| ADDU | 5129 | 5129 | 5129 | 5129 | 0.00% |
| SLL | 5156 | 5156 | 5113 | 5113 | 0.00% |
| SRL | 6287 | 5241 | 5625 | 5241 | 6.83% |
| SRA | 6287 | 5241 | 5625 | 5241 | 6.83% |
| SLLV | 6306 | 5249 | 5633 | 5249 | 6.82% |
| SRLV | 6311 | 5249 | 5633 | 5249 | 6.82% |
| SRAV | 6310 | 5249 | 5633 | 5249 | 6.82% |
| AND | 6297 | 5289 | 5641 | 5289 | 6.24% |
| OR | 6295 | 5289 | 5641 | 5289 | 6.24% |
| XOR | 6299 | 5289 | 5641 | 5289 | 6.24% |
| NOR | 6298 | 5289 | 5641 | 5289 | 6.24% |
| SLT | 6360 | 5353 | 5681 | 5353 | 5.77% |
| SLTU | 6336 | 5361 | 5665 | 5361 | 5.37% |
| SUBU | 6276 | 5241 | 5625 | 5241 | 6.83% |
| DADDU | 6301 | 5289 | 5641 | 5289 | 6.24% |
| DSUBU | 6300 | 5289 | 5641 | 5289 | 6.24% |

## Validation and open work

182/182 native regressions pass. Browser fixtures cover root/parent navigation, filtering, sorting, filenames with spaces, path-buffer refusal, more than 128 entries, missing folders, and mounting/reading a synthetic selected image through both existing mount APIs. Both final Wii ELF builds pass the inherited CPU/cache/IRQ/GS/config/IOP/VU0/VU1/VIF suites and the additional native font/XFB/image-filter PPC checks. Native guest checkpoint continuation advances EE 2,041,839,119 -> 2,045,035,061 with neither core halted, without forced guest state changes; this is a warm interpreter checkpoint check, not a fresh cold boot or hardware timing test.

Full multi-instruction EE block JIT, GS mixed min/mag/LOD texture filtering, complete VU flags/pipeline/Q/P/EFU and documented overflow NaN behavior remain open. No new Tekken gameplay test was performed. A real Wii controller/SD-browser run and measured FPS comparison are still needed. The public checkpoint contains source, DOL/ELF, exact cumulative patch/diff, handoff, previews and verification logs; no BIOS/game/guest RAM is included. Private warm-menu recovery state is saved separately.

Build with tools/build_r1276.sh using devkitPPC/libogc, -j1 and separate engine directories. Runtime identifies R1276.
