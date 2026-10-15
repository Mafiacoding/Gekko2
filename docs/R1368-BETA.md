# Gekko2 R1368 Beta

Based on R1367. This release applies two small, independently checked optimizations after reviewing the public NetherSX2-patch, EmuCoreX and ARMSX2 repositories. It does not import an Android renderer or ARM64 machine code.

## Changes

- EE cached successor selection validates a learned owner once. The fallback selects a budget-compatible variant without re-selecting an unsuitable learned owner. Owner identity, source/mapping generations, live first-word checks and per-instruction retirement guards remain. Active chains still pin code owners. This remains a guarded native call chain, not unconditional patched PPC branches.
- A single GS VRAM range hash is reused until an overlapping physical write. CT32, CT16/16S, indexed, depth, span and bulk writes invalidate it. Disjoint writes keep the digest. GPU work invalidates it and synchronization precedes reuse; failed resolvers cannot expose a stale digest. Readbacks are retained.
- GX's two read-only VRAM snapshots and checkpoint saving now use the bounded const accessor. Checkpoint restoration uses a synchronized tracked bulk write. An escaped mutable raw VRAM pointer permanently disables hash memoization for the process, including across cold boots, because an old caller can retain it.
- The existing GX source-cache option controls digest reuse. No new speedhack default, EE/IOP clock ratio, VU instruction omission or register-helper ABI change is introduced.
- Logs add `GS_HASH_CACHE hits=... avoided_bytes=...`. Hits mean an unchanged range avoided hashing, not GPU draws or FPS.

## Validation and measured scope

- 39 native regressions, including exact hash changes for every writable pixel-format family, disjoint writes, GPU resolver failure, option toggles, bulk writes and retained raw pointers.
- 126 linked-PPC native-chain cases: cold/warm budgets, modified successor instructions, timer boundaries, nonvolatile-register ABI and allocation failure.
- 12 mixed-owner prefix cases: complete EE/timer/IRQ/display state, following RAM writes and code-arena pressure.
- Linked-PPC GX source packing and fresh scanout flush checks. GPU commands are mocked; no claim of physical GX output validation.
- Scheduler comparison covers EE/IOP state, timers and shared mailbox accesses.
- Final DOL section bytes are checked against its ELF, including Wii startup markers.

Warm two-owner EE jump loop, 100 calls of 32 instructions after warming: R1367 1,747,849 PPC instructions; R1368 1,689,349 (3.35% less). Both retire 3,328 total guest instructions and produce the identical full EE-state SHA-256 `9f8979228310acad3495c204105a2fb8b8f2c31c3dddefb5c7e6f4e95f3eebc0`.

Eight digest requests for one unchanged 16-KiB range: R1367 295,216 PPC instructions; R1368 37,364 (87.34% less), with identical digests. A miss still hashes the entire range; tracked writes incur a small invalidation check. These are synthetic CPU-path instruction counts, not Wii wall-clock measurements or a whole-emulator speedup.

## Wii test

Use the same settings and BIOS/game as the preceding R1367 run. Keep GX source cache enabled to test digest reuse; use a cold boot. Compare first-image elapsed time and stable EE_per_s/guest_vblank values at equivalent guest progress. Send the full BIOS log and, separately, a Tekken log. Check `GS_HASH_CACHE` alongside `GX_SOURCE_CACHE`; zero hits may simply mean that textures change between requests. No physical Wii boot or FPS improvement has been confirmed for this release.

## Reference scope

- https://github.com/Trixarian/NetherSX2-patch at f5f89b3b76e89a8f684da91cf5ad6e542825c0cb: patch/configuration project, not a public native recompiler source. Game profiles are a later task, not imported here.
- https://github.com/sashkinbro/EmuCoreX at 8713f703d5154dc324024fc9ad98a8535471d3d1: native PCSX2-derived register allocator, load/store timer event tests and GS texture-cache dirty rectangles. The present change uses bounded physical-range invalidation, not its complete target/texture cache.
- https://github.com/ARMSX2/ARMSX2 at e327f0d38ca61dba2be2e0cd0b7785e8436f730a: native EE dispatcher/linker, register pin handoff and VU program-cache mechanisms. These inform owner validation; no ARM64 emitter code was copied.

Selective deferred register writeback, persistent VU code, full GS target residency and per-game profiles remain separate work. Existing ARM-worker limitations are unchanged.
