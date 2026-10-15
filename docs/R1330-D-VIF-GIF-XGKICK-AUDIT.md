# R1330-D — VIF / GIF / XGKICK compatibility audit

Base: `25ac11cb6a91448dd4462f4d2d0807e5f56d1301` (R1330-C fully green).

## Scope

This batch audits the game-facing VIF1 -> VU1 -> XGKICK -> GIF/GS path. It does not move EE/IOP/VU control flow to GX and does not change the separate R1329-B SCE boot-livelock diagnostics.

## Confirmed existing coverage

### VIF

`source/hw/vif.c` already implements the command families needed to feed VU memory/micro memory and start VU execution, including STCYCL/OFFSET/BASE/ITOP/STMOD/MASK/ROW/COL, MPG, MSCAL/MSCALF/MSCNT, DIRECT/DIRECTHL and UNPACK. The UNPACK implementation already has explicit split-payload/carry handling and bounds guards. Therefore R1330-D must not replace this with a title-specific fast path.

### VU1

`source/hw/vu.c` has an explicit per-unit pipeline path, Q/P publication/stalls, branch/E-bit delay handling, interpreter fallback and VU JIT block/pair attempts. MSCNT resumes from the existing TPC rather than resetting to zero.

### GIF

`source/hw/gif.c` already distinguishes PATH1/2/3 at the packet parser entry and has packet carry state. The GS side is much broader than the old header scope text implies; R1330-E will separately audit rendering semantics. R1330-D is concerned with delivery/order/stall boundaries before rasterization.

## Confirmed R1330-D gap: XGKICK is synchronous and unmetered

The current XGKICK lower-op handler in `source/hw/vu.c` immediately calls `gif_process_quadwords(GIF_PATH_1, ...)` over the VU1 memory ring. The source comment explicitly documents that real hardware's asynchronous `xgkickenable/xgkickaddr/xgkickdiff` queueing and metered drain are not modeled.

This is a real generic game-compatibility boundary, not missing native Dynarec code and not a GX problem.

Consequences of the current shortcut:

1. VU1 cannot represent an outstanding PATH1 kick while subsequent microinstructions continue.
2. A later XGKICK cannot stall behind an earlier unfinished PATH1 transfer.
3. PATH1 delivery can be observed by GIF/GS immediately at the issuing lower instruction rather than through a separate transfer boundary.
4. The current call gives the GIF parser the remainder/full VU1 ring and relies on packet parsing to stop; this is safe for bounds but does not model transfer progress.

## Implementation direction

R1330-D should add a small deterministic VU1 XGKICK transfer state rather than guessed wall-clock timing:

- capture the XGKICK start qword address from `Is & 0x3ff`;
- mark PATH1 transfer pending/active instead of parsing the whole ring directly in the opcode handler;
- drain through a bounded service function at explicit VU execution boundaries;
- preserve ring wrap without copying/reading outside VU1 memory;
- prevent a second XGKICK from overwriting an outstanding transfer; it must stall/defer according to the modeled busy boundary;
- keep VU0 XGKICK disconnected;
- keep GIF parsing and GS/GX rendering behind the existing `GIF_PATH_1` API;
- retain deterministic CPU fallback and do not introduce title checks.

Do not invent a per-qword hardware cycle count in this batch unless repository/trace evidence establishes it. Correct ordering and busy/stall state are the first target; timing precision can be refined from runtime traces later.

## Regression requirements

Focused tests must cover at least:

1. XGKICK is VU1-only.
2. Start address uses `(Is & 0x3ff) * 16`.
3. Ring-boundary packet delivery remains valid.
4. Outstanding PATH1 state cannot be silently replaced by a second kick.
5. GIF receives PATH1, not PATH2/PATH3.
6. Existing VIF MSCAL/MSCALF/MSCNT and split UNPACK tests remain green.

Promotion gate remains the standard R1330 gate: focused regressions, complete isolated host suite, devkitPPC/Wii cross-build and ELF/DOL verification.