# BETA12.13 — GS eDRAM physical aliases / GX framebuffer coherence

Base: BETA12.12 GS READBACK; retains all previous native timing and transfer paths.

## Implemented

1. Replace the generic 64x32 eDRAM page envelope in GIF caching with conservative native PSM page geometry: CT32/24 and high-index PSM = 64x32, CT16/16S and Z16/16S = 64x64, T8 = 128x64, T4 = 128x128. Z32/24 remain 64x32. All views address shared eDRAM, not independent texture allocations.
2. On textured GX sprite source capture, perform physical byte-envelope GPU ownership synchronization *before* hashing/sampling. A proven-disjoint sample does not require GPU readback; an overlapping one resolves pending GX work. Readback failures invalidate the source and remain visible instead of silently reporting success.
3. GS TRXDIR=2 physical overlap with different PSM / BP / BW can now use a 16,384-pixel (64 KiB) source snapshot before any destination write, preserving source pixels under eDRAM layout reinterpretation. Larger unsafe alias moves still fail closed. Existing same-layout directional copies remain unchanged.
4. Fix GS color/depth 16-bit physical 8 KiB page-range detection (`64x64`, not `64x128`), which previously produced incorrect alias decisions.
5. Add `GS_ALIAS_SYNC queries resolves disjoint failures cross_layout_snapshots snapshot_pixels size_declines` to diagnostic logs.

## Tests performed

- Native `gcc` syntax check of GS and GIF modules.
- `test_gs_alias_sync_beta12_13`: PSM16 page bound; CT32->Z32 cross-layout physical overlap and data integrity; size guard; deferred GPU ownership disjoint/resolved/failure.
- `test_gs_alias_gif_beta12_13`: four native page geometries and GPU->CPU texture readback through real GIF texture-source hashing; GIF local move regression.
- Regression fixtures for `TRXDIR=2`, `TRXDIR=1` / GS FIFO.
- devkitPPC / libogc Wii cross-build with preexisting SPEED, DSI and ARM compile flags; DOL/ELF code equivalence and static MEM1 check.

## Limitations

- Physical aliases beyond the bounded snapshot size are explicitly declined. This is not a complete implementation of every possible GS overlap / 4MiB wrapping case.
- GX residency still targets selected PSMCT32 draws; alias-aware handling for every arbitrary GS render format and all readback hazards remains incomplete.
- No automatic front/back framebuffer flip is forced. Legitimate fades or black frames remain untouched.
- No on-console hardware test has yet confirmed Crash E3 gameplay, BIOS, GT3 or Tekken rendering improvement; no FPS claim.
- Build is diagnostic `-O1`, not appropriate for direct comparison with older `-O3` measurements.

## Research references

- PCSX2 GSRendererHW: https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS/Renderers/HW/GSRendererHW.cpp
- PCSX2 GSTextureCache: https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS/Renderers/HW/GSTextureCache.cpp
- PCSX2 GSState: https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS/GSState.cpp
