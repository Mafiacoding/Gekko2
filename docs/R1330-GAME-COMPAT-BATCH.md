# R1330 Game Compatibility Audit Batch

Base: `dynarec-development-r1308` @ `328dcb86aab3ae0f792168a73385cda1f5b428fd`

This branch is deliberately isolated from R1329-B boot-livelock diagnostics. Do not change EE IRQ/timer/scheduler semantics here to work around the SCE boot blocker.

## Goal

Advance game compatibility work that can be verified statically and by host regression while physical Wii/Dolphin runtime validation is unavailable.

## Batch order

1. **Disc image / ISO / BIN / CDVD path**
   - Audit ISO9660 traversal and sector bounds.
   - Audit 2048 / raw 2352 Mode 1 / Mode 2 Form 1 handling.
   - Audit SYSTEM.CNF path/version handling and game ELF discovery.
   - Audit the boundary between standalone ISO parsing and the live IOP CDVD model.
   - Do not make the BIOS-only boot path report a disc merely to bypass R1329.

2. **EE ELF/game loader**
   - Validate ELF32/MIPS program-header table bounds and malformed-image rejection.
   - Validate PT_LOAD file/memory size relationship, address/range overflow and BSS clearing.
   - Validate entry/load-range reporting and cache/JIT invalidation requirements when game code is installed into EE RAM.

3. **SIF/CDVD RPC and game I/O**
   - Audit game-facing CDVD commands, async completion, DMA destinations and IRQ completion.
   - Distinguish intentional HLE/hardware fallbacks from missing behavior.
   - Add focused host tests for any proven gap before implementation.

4. **VIF/GIF/XGKICK path**
   - Audit VIF unpack/MSCAL/MSCNT/FLUSH semantics used by games.
   - Audit VU XGKICK PATH1 transfer ordering, stalls and GIF arbitration.
   - Preserve deterministic CPU fallback; do not hide missing semantics behind GX.

5. **GS compatibility / GX-safe preparation**
   - Audit texture state, Gouraud/textured triangle state, alpha/depth tests and framebuffer dependencies.
   - Fix CPU-side GS semantic gaps first.
   - GX remains optional/feature-gated; no EE/IOP/VU control flow is moved to GX.

6. **TLB/game-memory regression**
   - Audit game-facing mapped accesses and exception behavior relevant to the previous Tekken TLB read failure.
   - No address hacks or title-specific bypasses.

## Verification gate for every implementation batch

- Focused regression test(s) for the exact gap.
- `bash tests/run_test.sh --all`
- devkitPPC/Wii cross-build.
- No merge/promote to `dynarec-development-r1308` until green.
- Runtime/game compatibility claims require later Wii/Dolphin validation.

## First concrete audit findings

- `iso_loader.c` already detects plain 2048, raw 2352 Mode 1 and raw 2352 Mode 2 Form 1 by probing the ISO9660 PVD.
- `iso_find_path()` already supports nested ISO9660 directories, optional `cdrom*:` prefix and a leaf `;1` fallback. The old header scope text saying only single-level lookup is stale and must not be treated as an implementation gap.
- The live CDVD wiring remains intentionally separate from the standalone ISO parser and must be audited rather than enabled speculatively.
- `ee_elf_load()` has real hardening work to audit: program-header table arithmetic, `p_filesz <= p_memsz`, integer-overflow-safe image/range checks, and explicit executable-code/JIT invalidation semantics after loading.

Next implementation batch starts with the EE ELF loader because its correctness can be proven with host tests without requiring the blocked BIOS runtime path.
