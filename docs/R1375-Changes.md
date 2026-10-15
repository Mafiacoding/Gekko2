# Gekko2 R1375 A/B

R1374 B used native additive GX blending, but the physical Wii still crashed in the BIOS animation. The log does not identify the faulting instruction. This build reduces readback work and tests a stricter resident GPU completion boundary. It does not claim the crash is already fixed.

## Changes

- Track a dirty bounding rectangle for resident render surfaces.
- Copy only the 4-pixel-aligned dirty rectangle from EFB, rather than the entire render target; import with the rectangle's own RGBA8 tile stride.
- Preserve exact CT32/CT24 alpha shadow and all untouched VRAM bytes.
- Release seed-only targets without a pixel readback, after draining queued GPU reads.
- Use a separate readback allocation so resolve does not overwrite the seed texture allocation.
- Retain R1374 JIT budget lookup changes, DSI fastmem and Lightrec pilot. No new ARM worker or EE/VU ARM offload.
- No frequent SD dump writer. Normal logs remain.

## Variants

A disables resident blending, as R1374 A did. It includes bounded readbacks.
B keeps bounded native additive blending and explicitly calls GX_DrawDone after each resident textured submission. This fence may reduce speed; it tests completion and ownership boundaries. It is not a verified physical Wii crash fix.

## Test

Start A first with the same BIOS and options. Save its log separately. Then cold boot B with the same options; do not change GX during the run. Copy the log even if it hangs. Both use session filename R1375; BUILD identifies A/B.

New log: GX_BOUNDED_READBACK avoided_bytes, seed_only_releases and resident_fence (A=0, B=1). Readback counters reflect the bytes actually copied. Seed uploads still cover the full target; this revision does not eliminate target churn or CPU texture shading.

## Validation

42 native test programs passed, including 160 overlapping surface draws and 100 varied partial-import cases. Partial imports preserve full VRAM, mixed CT32/CT24 alpha and atomic rejection of invalid rectangles/capacities. Linked PPC tests with mocked GX calls verify A rejection, B additive submission, the smaller aligned resolve copy, pending ownership release and completion fences. DOL sections match their ELF and stay inside MEM1. GCC 16.1.0 / libogc 3.1.0.

No local physical Wii or Dolphin run; GX calls in linked tests are mocked. EFB pixel output, crash stability and FPS must be tested on hardware. No promised speed multiplier.
