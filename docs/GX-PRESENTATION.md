# Gekko2 experimental GX renderer — R1302

Guest graphics remain at the R1300 checkpoint. New GX features are paused while custom PPC dynarec work takes priority. [STATUS](../STATUS.md) gives the current evidence; [R1300 handoff](R1300-HANDOFF.md) describes the exact resident/snapshot implementation.

## Control and scope

There is one opt-in **GX (EXPERIMENTAL): ON/OFF** setting; LEFT or RIGHT toggles presentation and eligible hardware drawing together. OFF is the default. Activation waits for the first real BIOS image. Requested GX mode does not prove that every draw or frame uses GX.

Supported paths include PSMCT32/24 presentation, eligible sprites/flat triangles, guarded texture-snapshot drawing, selected exact integer TEV blend/alpha cases and compatible depth tests. R1299 adds FIX coefficient coverage, uniform AS/AD specialization, complementary PABE passes, Z16/Z16S handling and split Z32 comparison. Unsupported GS states remain software or hybrid.

R1300 supports compatible resident EFB targets, matching GPU-only scanout with raw RGB restoration, and validated source-snapshot reuse. Varying Gouraud/textured triangles, general texture functions/filtering on GPU and broad framebuffer residency remain incomplete. CPU snapshot preparation, exact coverage/state decisions, shadow metadata and fallback still carry cost.

## VRAM ownership

Shared PS2 VRAM remains coherent with GPU-owned dirty regions. CPU overlapping reads/writes, raw access, aliasing and unsupported state transitions resolve first. Guarded nonalias access may avoid resolution. CT24 destination alpha must survive GPU transfers. Snapshot keys include validated source/state/CLUT/dimensions/sampler information; writes invalidate reuse.

Compatible scanout backs up raw EFB state, applies the existing broadcast RGB clamp before output filtering, presents and restores raw RGB. Do not reuse scanout-filtered pixels as guest VRAM or replace guest data with a display-only image.

## Validation and limitations

Earlier owner logs demonstrate GX presentation/boot, but cannot establish current hardware primitive coverage or speed. Linked-PPC and synthetic EFB tests verify routing, ownership/readback, cache behavior and state parity; they do not execute a physical Wii GPU. R1302 has no new Wii performance result. Preserve software fallbacks and report accepted hardware/hybrid draw counters separately from requested mode.

Relevant source: `source/hw/gs_gx.c`, `gs_mem.c`, `gs.c`, `gs_wii_output.c`. Read R1297–R1300 handoffs before changing alpha, sprite bounds, depth, barriers or residency.
