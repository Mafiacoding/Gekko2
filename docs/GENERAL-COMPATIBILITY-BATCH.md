# General compatibility work: six requested emulator areas

19 October 2026. Branch `codex/general-compatibility-batch`, on top of
`codex/alpha-ee-masked-windows` (PR #48). Runtime source commits `ac420e5`
and `9d14d74`. No version bump, published download or alpha-readiness claim.
The owner's release hold remains in effect. These are specific corrections,
not completion of the four emulator subsystems.

## 1. IOP modules, SIF and RPC

The IRX loader previously rejected legal standalone LO16 and Sony linked MHI16
relocations. It now handles NONE, 16, independent LO16, and MHI16/ADDEND. The
MHI16 chain uses signed relative word links and validates its entire extent and
termination before overwriting any link. Missing/wrong ADDEND, truncated REL
entries, out-of-module links and cycles are rejected. Existing paired HI16/LO16,
32-bit and jump relocations remain supported.

Behavior reference: ps2sdk `iop/system/loadcore/src/loadcore.c`,
[ApplyElfRelSection, commit 329e2bb](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/iop/system/loadcore/src/loadcore.c#L1323),
AFL-2.0. The reference was inspected outside the repository; its source was not
imported. This extends loading, not on-demand execution of arbitrary game IRX
modules. LOADFILE/RPC's HLE routes, module lifecycle and full SIF timing remain
separate gaps. No title-support claim follows from relocation coverage alone.

## 2. EE/IOP scheduler and DMA

The parent change admits native EE windows with **stable masked** INTC/DMAC
levels, while retaining visible Cause bits. Deliverable IRQs and level
transitions still take precise paths. This batch adds actual linked PPC
differential execution: complete EE, timer, display-clock and IRQ states agree
in twelve masked-level cases. Existing DMA, mapping, code invalidation and VU/GIF
barriers remain. Native events and event batches remain factory OFF.

The host diagnostic runner no longer silently forces event-scheduler and idle
batch bits after reading `GEKKO2_OPTIONS`. It validates masks strictly and prints
the actual selected value. This makes legacy/event comparisons meaningful;
it does not change Wii defaults or fabricate device progress.

Synthetic warm ADDIU cost examples, measured as **executed PPC instructions**:

| Workload | Precise path | Native event path |
| --- | ---: | ---: |
| 8 instructions, stable masked INTC | 6,427 | 1,992 |
| 32 instructions, stable masked INTC | 24,551 | 6,811 |
| 8 instructions, stable masked DMAC | 7,355 | 1,992 |

These are not Broadway cycles or overall Wii FPS. The wider scheduler regression
also measures a cold-compilation penalty: at 257 slots, legacy costs 4,343,725
versus event 8,863,516 PPC instructions. Its warm repeated workload favors event
batching. Therefore promoting the experimental mode globally is not justified.
Instruction-slot time remains an approximation; a complete cycle-driven device
queue and every asynchronous DMA/SIF wakeup are not implemented by this batch.

## 3. VU, VIF and GIF

VU ADD/SUB micro arithmetic now discards the smaller input when raw exponents
differ by at least 25 steps, following the inspected vendored
`VUops.cpp::vuADD` behavior. Host IEEE addition previously produced an adjacent
float for opposite-sign operands close to a power of two. Vector, broadcast,
Q/I and accumulator forms share the correction. Timed arithmetic uses the
shared implementation; untimed paths select it only for affected ADD/SUB input
edges, with pair-native execution suppressed to avoid double retirement.

This is one numerical alignment rule, not full hardware mantissa precision,
nor a VU0 macro COP2 fix. MADD/MSUB are unchanged. No new XGKICK transport fix is
asserted: existing real-PPC wrap/live-tag and VIF barrier checks were rerun.
XGKICK transports GIF data; it does not transform geometry or replace GS.
VI/CLIP/M synchronization and full pipeline ordering still need broader proofs.

## 4. GS and GX

Textured LINE and LINE_STRIP previously ignored texture attributes in the
software line rasterizer. The vertex queue now retains UV or S/T/Q, including
XYZ3 queue advancement and strip carry. It interpolates homogeneous coordinates
before division and feeds the shared sampler and TFX/TCC stages. Existing fog,
alpha, depth, target masks and ownership barriers remain. Both GS contexts are
covered. It reuses checkpointed primitive-lane storage without changing the
serialized state layout.

This complements the parent's textured POINT correction. It does not add a
native GX line accelerator or precise GS subpixel/AA1 coverage. Complex native
GX feedback, mip paths and EXTWRITE remain open; no experiment is enabled by
default and no geometry, IRQ or fence is bypassed.

## 5. BIOS tiles: controlled negative result

The private PCSX2 BIOS GIF stream was replayed before and after line texturing:
408 VSYNC events, 158,693 transfers, 377,026 triangles, 9,497 sprites, 340 points
and 18,340 lines in each run. All 77 retained scanout images were byte-identical.
Thus the line correction does **not** fix the tile scene in this captured stream.
Replay bypasses EE/VU, so it does not validate the new VU arithmetic change.

Independent official PCSX2 v2.8.2 software replay of the same owner's BIOS stream
also lacks the bright small squares in the supplied screenshot. That does not
establish that Gekko2's BIOS rendering is correct: reference BIOS/NVM/card state,
animation phase and input packets still need matching. No isolated color-depth,
XGKICK or geometry cause is established. All owner BIOS, captures and derived
data remain outside Git; no artificial tiles or global brightness changes.

## 6. GT3 after the first screen

The owner supplied sixteen RAR volumes during this work. Volume 16 transferred
successfully (18,798,989 bytes); volume 1 was rejected by this environment's
fixed **32 MiB attachment-download limit**. The other volumes are not assumed
complete or validated, and no complete image is accessible yet. A direct owner
download link was requested while independent work continued. The owner then
provided an Internet Archive URL labeled Japan. The environment's enforced
allowlist rejects `archive.org` with proxy CONNECT 403; an additive domain change
was saved in the environment draft, preserving GitHub API access. It requires
user review/save/publication and does not establish running network access.
Region and image integrity have not been verified. There is no new GT3
boot/progression result; historical copyright/card screens are not gameplay
evidence. No game data is published or replaced with synthetic assets.

## Validation

Toolchain: devkitPPC r50 / GCC 16.1.0, libogc 3.1.0, `-ffp-contract=off`, existing
SPEED/AOT/DSI/idle/GX fence flags. Internal chain and control builds retain the
BETA12.33 label and embed runtime source `9d14d74`. Build commands:

```sh
bash tools/build_beta12_33_windows.sh
CHAIN_RESIDENCY=0 bash tools/build_beta12_33_windows.sh
python3 tools/test_alpha_compat_host.py --output-json /tmp/alpha-host.json
```

Linked checks use `.venv-gekko2` Unicorn 2.1.4 and the actual target ELF, e.g.:

```sh
python tools/verify_vu_add_alignment_linked.py Gekko2-BETA12.33-GX-Chain.elf \
  --nm "$DEVKITPPC/bin/powerpc-eabi-nm"
```

Unicorn executes target PPC arithmetic and guest-state code; SDK allocation,
cache maintenance and FP context services are modeled. It is not Dolphin or a
physical Wii. Final counts and hashes are in the verification record linked
below. No new physical Wii/Dolphin run, matched game progression, tile-fix or
overall FPS claim is made.

Executed final gates on runtime `9d14d74`:

| Check | Result |
| --- | --- |
| Host wrapper | 89/89 groups PASS |
| Added host assertions | EE masked 1,557; POINT 8; IRX 774; LINE 44; VU ADD/SUB 9,600; seven driver mask cases |
| Actual linked PPC, each of two ELFs | Nine groups PASS: masked windows, textured lines, IRX, GS/VIF accuracy, event scheduler, XGKICK/IPU, VU alignment, timed FMAC and hybrid VU |
| VU exact alignment, each ELF | 512 programs / 3,072 lane and flag checks PASS, scalar and block-option runners |
| Legacy differential scheduler, each ELF | 26 full EE/IOP/timer comparisons plus six mailbox ordering cases PASS |
| Existing VU timing/hybrid regression, each ELF | FMAC queries, FSSET/FDIV, RAW/Q, budgets, invalidation and ABI PASS |
| DOL/ELF section identity and static MEM1 | Both PASS; chain end 0x815e62f4, 2,202,892 bytes remaining; control end 0x815c4cf4, 2,339,596 bytes remaining |
| Old VU arithmetic negative control | Same host regression compiled against ac420e5 fails the expected result assertion |

[Synthetic results and internal build hashes](verification/GENERAL-COMPATIBILITY-RESULTS.json).
Historical `verify_beta12_23_linked.py` was also attempted on the preceding
ac420e5 ELF: its seven VIF barriers and sixteen fractional-UV cases pass, but
its old pillarbox stage fails setup because `gs_xfb_fit_4_3` is linker-stripped
after the later fullscreen correction. That complete historical script is not
counted as passed. The standalone mocked DMA CALL/RET fixture likewise cannot
use the generic all-core test linker because it defines its own device mocks;
that attempt is not a passing regression result. Neither limitation was hidden
by removing assertions or disabling runtime barriers.
