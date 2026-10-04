# R1286 — GS color/depth bounds fix

Recovered the complete R1285 source from the saved manifest/CRC-verified checkpoint and its cumulative patch against the original uploaded archive. No older R1270 source is used for this release.

## Defect and fix
Scalar PSMCT32, PSMCT16/16S, compatibility swizzled color and Z-buffer helpers checked offset + width > capacity. That sum wraps at UINT32_MAX. In the actual R1285 PPC ELF, bp=0x3fffffff,x=y=0 produces offset0xfffffffc: the guard passes and pointer arithmetic reaches four bytes before shared VRAM. A canary proves both invalid read and invalid write.

R1286 checks offset > capacity - width. Width is fixed2/4 or selected2/3/4 for Z. Existing valid swizzled addressing, byte order and last legal accesses are preserved. Read/span/fill helpers already used safe checks. Coordinate/address arithmetic still follows its previous uint32 semantics; this patch fixes the guard rather than changing address wrapping or claiming full GS hardware accuracy.

## Validation
- Native suite:184 tests, including a new GS bounds regression. All eight helper families have48 native boundary cases, also run with AddressSanitizer/UndefinedBehaviorSanitizer. Leak detection is disabled because this environment's process tracing prevents LeakSanitizer; the address/undefined checks remain enabled. The helper uses static buffers and no heap allocations.
- Both actual Wii ELFs:42 color/depth boundary cases each, prefix canary and full-VRAM preservation on invalid writes, final valid little-endian16/24/32-bit accesses. The unreferenced swizzled compatibility API is removed by linker GC and is covered natively instead of pretending its PPC symbol exists.
- Both builds retain profile/cache/IRQ/rendered STQ checks,72 full-memory row-fill byte oracles and24 GX texture-pack cases.66 valid rendered workloads per build have identical entire-VRAM signatures to the R1285 baseline.
- No new Wii hardware/FPS or BIOS-menu proof. The R1285 warm continuation remains prior evidence; no new cold boot is claimed.

## Installation and status
Rename PCSX2-Wii-R1286-Menu-JIT.dol to sd:/apps/pcsx2-wii/boot.dol. Preserve BIOS/config/discs. Log:sd:/pcsx2/R1286-boot.log. GX remains opt-in (Settings RIGHT), hardware-unverified presentation only. Full JIT blocks/linking/register allocation and GS primitive GPU rendering remain open.

## Distribution
Public source/patches exclude BIOS, games, private guest RAM/checkpoints, compiler/SDK and personal runtime logs. Cumulative Claude patch/diff target the original uploaded project. Incremental fix patch targets R1285; GitHub source patch targets the original repository commit68964c8a6ddc08fb4c470e9271cefa5e3d106394. Existing history is preserved. Repository:https://github.com/Mafiacoding/PCSX2-Wii.
