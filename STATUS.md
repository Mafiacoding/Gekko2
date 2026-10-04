# Gekko2 status — R1302

Updated 2026-10-04. **Early alpha; alpha coming soon, without a fixed date.** This is the current project status. Historical round documents describe their own revisions and do not override it.

## Verified progress

| Area | Evidence | Remaining limits |
| --- | --- | --- |
| Real Wii BIOS | Owner confirmed the Sony Computer Entertainment startup screen and Wii Remote idle fix. Both earlier GX variants booted in owner testing. | Stable, responsive OSDSYS navigation and current-build FPS need hardware confirmation. |
| Native OSDSYS | BIOS menu renders; Browser entry and return were tested in native checkpoint work. | A native framebuffer capture is not evidence of current Wii speed or stability. |
| Tekken Tag Tournament | Namco logo reproduced in native tests and reported on Wii by the owner. R1297 retail SLUS-20001 resource-transfer fix passes the previous TLBL failure in fresh native boot; fractional sprite gaps are fixed. | Title screen/gameplay and playable performance are not demonstrated. The custom module profile is guarded, not a universal sound-module implementation. |
| Launcher/input | SD ISO/BIN browser, FPS/HUD options, Remote/Nunchuk/GameCube controls and HBC exit actions are implemented. | Nunchuk directions are digital; CHD launcher support is absent. |
| PPC JIT | Many scalar EE/IOP families, guarded VU pairs/blocks, conservative 2–8 instruction EE ALU/COP1 blocks and direct relative helper calls. | Full EE/IOP/VU recompilation, larger memory/control blocks, register allocation and safe linking remain incomplete. |
| GX | Optional CT32/24 presentation, guarded sprites/flat triangles, snapshot textures, selected TEV blending/depth paths, deferred VRAM resolve and compatible resident framebuffers. | Varying Gouraud/textured triangle routing, broad GPU residency and complex GS states still require software/hybrid handling. |
| R1302 branding | Gekko2 launcher header, HBC icon, checked 640×480 native launcher preview and both Wii cross-builds. | No fresh physical-Wii R1302 timing result. Guest emulation remains R1301. |

## Latest changes

- **R1297:** guarded Tekken retail resource transfers, fractional sprite coverage and precise COP1 block admission. [Details](docs/R1297-HANDOFF.md).
- **R1298–R1299:** snapshot sprites/flat triangles, exact scoped TEV blend cases, PABE specialization and split depth handling. [R1298](docs/R1298-HANDOFF.md), [R1299](docs/R1299-HANDOFF.md).
- **R1300:** guarded resident EFB surfaces, matching GPU scanout/raw restore and validated snapshot reuse. CPU overlap/unsupported states resolve to shared VRAM. [Details](docs/R1300-HANDOFF.md).
- **R1301:** Gekko2 naming and reachable direct relative PPC helper calls with far/unaligned fallback; all instruction-boundary checks retained. [Details](docs/R1301-HANDOFF.md).
- **R1302:** embedded launcher gecko wordmark and HBC icon. Guest runtime unchanged. [Details](docs/R1302-HANDOFF.md), [artwork](docs/BRANDING-R1302.md).

## Validation record

R1301 passed **200/200 native tests** and four linked-PPC jobs across Interpreter/JIT builds. The linked paths preserve **66 matching full-VRAM signatures**, ALU/COP1 state parity, and checks for budgets, interrupt boundaries, source mutation/TLB replacement and far callback fallback.

R1300 additionally checked resident framebuffer ownership and snapshot reuse with synthetic EFB/linked-PPC tests. These test services validate control/state behavior; they do not substitute for a physical GX GPU or establish Wii rendering speed.

R1302 passed both devkitPPC builds and native launcher visual inspection. The guest suite is inherited from R1301, **not rerun for the artwork-only revision**. See [public verification summary](docs/verification/R1302.json).

Warm eight-instruction R1301 benchmarks counted 45 fewer PPC instructions: ADDIU 3240→3195; mixed COP1 3226→3181. These isolated counts are not Wii cycle measurements or an overall FPS improvement.

## Performance and next work

No 10 FPS BIOS or 15–20 FPS Tekken target has been demonstrated. FPS counts emulated VBlank events per host second; OUTPUT counts host presentations, which can repeat the same scene. Neither counts unique images. Older logs and synthetic tests cannot predict current real-Wii FPS.

GX feature work is paused while the custom PPC dynarec becomes the priority. Next: profile-guided register residency/allocation, guarded memory/control blocks and corresponding IOP work, with precise interrupts, delay slots, exceptions and code invalidation. Keep the BIOS stable before expanding game work. Wii64/nullDC4Wii are reference projects, not integrated cores; Lightrec has not been adopted.

## Distribution and reporting

Public sources contain no BIOS, discs, private guest RAM/checkpoints or owner runtime logs. Preserve original source and third-party notices. Report revision, BIOS model, controller, JIT/interpreter and GX/software settings with timings/screenshots; redact personal paths and never share protected firmware/disc data.
