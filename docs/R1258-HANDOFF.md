# R1258 checkpoint — OSDSYS setup and Tekken Namco rendering

Confirmed results (native execution of real EE/IOP core, no forced PC, guest flags, completion counters, or fabricated screenshots):

- Fresh diskless PACKED-fix run reaches the real OSDSYS language setup at EE=1600256720. Frame shows Select language / Sprache wählen and X Enter. This is the first-run setup, not yet the Browser / System Configuration menu.
- Tekken Tag Tournament SLUS_200.01 from the supplied raw 2352-byte BIN reaches its genuine no-memory-card dialog, then PRODUCED BY namco after normal PS2 START input. Fresh Interpreter test completes 1000M EE without halt; logo visible in the actual framebuffer. The later VSync poll at 0x00400660 still waits (caller RA=0x003f708c, INTC 8/1006 at final sample). No game title screen or gameplay claimed.

## Corrections

1. PACKED TEX0_1/TEX0_2 and CLAMP_1/CLAMP_2 were silently ignored. Apply their natural low 64 bits through the same GS register handlers as A+D, matching the bundled PCSX2 GSState.cpp ResetHandlers. Verified split DMA packets, two contexts and real textured sprites.
2. XYZ2 vertex assembly selected XYOFFSET too late after CTXT switches. Select the effective drawing context before building each vertex. The prior first vertex used the previous context offset; alternating-context raster test covers this.
3. ISO9660 module lookup now walks nested directories, handles cdrom:/cdrom0:/cdrom1: paths and leaf ;1 fallback, rejects malformed/traversal paths and truncated records. Actual requests include cdrom:\IRX\MCMAN.IRX;1 and MCSERV.IRX;1.
4. Parse bounded little-endian SHT_SCE_IOPMOD metadata from actual requested ROM/disc modules. INIT 0xfe/0x70 returns those actual versions: supplied disc MCSERV=0x208, MCMAN=0x20b (mcman_tool). Previously zero versions made Tekken reject SDK initialization and clear its RPC server descriptor. LOADFILE remains HLE; metadata discovery does not execute the IRX.
5. MCSERV GETINFO 1/0x78 now models the cardless result and callback DMA outputs, instead of unconditional success. Supplied MCMAN 2.0b's McDetectCard2 export at text+0x824 falls through failed PS2/PS1 probes; the five failed PS1 probes return -11 at text+0x91e4. Type/free/modern format outputs are zero for absent cards; bounded physical SIF DMA writes precede ordinary guest REND callback. Inserted-card/filesystem HLE remains incomplete. No synthetic card insertion to bypass game code.
6. Wii START alone now reaches PS2 START. Z+START toggles diagnostic HUD, suppressing both chord buttons from guest input. B+Z returns to launcher, launcher START resumes. GameCube A maps to PS2 Cross for OSDSYS setup.

## Validation and limitations

165/165 regression tests pass. Both actual PowerPC ELFs pass 53 GS memory, 19 CLUT, 6 SPR, 9 MFIFO, 5 packed/context rendered-image and 4 IRX metadata checks. Results are included under verification in the archive. Both Interpreter/JIT ELF and DOL built with devkitPPC r32. JIT functionality/performance is not claimed improved or validated by a Dolphin boot; no Dolphin/Wii runtime is available here.

The private VU FMAND/FC/FS consumer experiment is not in this release: arithmetic MAC/status production and latency still need implementation and validation. OSDSYS frame indicates real setup text already works without that experiment.

Private runtime checkpoints do not retain static PADMAN binding globals. A resumed checkpoint cannot validate controller input faithfully; use fresh input tests. R1258 appends MC version fields to EE state, so use matching-build checkpoints only.

Package excludes BIOS, game image, extracted IRX/game ELF, guest RAM dumps and private runtime checkpoints. Patch/diff applies to original pcsx2-wii-full-debug(1).zip; this is a cumulative patch, not an incremental R1257 patch.
