# R1384 Codex: timing, arbitration and register residency

This is a substantial continuation of R1379-AOT3, with the actual R1383 source
recovered and compared. It is an experimental implementation checkpoint, not
proof of complete PS2 hardware accuracy or a finished all-native dynarec.

## Implemented

- Per-unit, serialized four-entry FMAC flag queue with four VU issue clocks of
  latency. Arithmetic covers scalar/broadcast/Q/I, ACC, MADD/MSUB and outer
  product forms. VF00 destinations still generate flags. Selected lanes set
  Z/S/U/O MAC masks; current and sticky STATUS bits retire separately from Q.
- Hot VU arithmetic uses exact-word native AOT specializations for the timed
  datapath as well as the existing raw handlers.
- FDIV no longer snapshots old sticky flags. Publication adds only the newly
  issued I/D flags. FMAC/FSSET retires before FDIV at a shared ready clock.
  FSSET snapshots the issued MAC/STATUS, not older visible flags. E drains
  pending flag results. The existing explicit Q/P wait/issue state is retained.
- VF RAW scoreboarding: a dependent pair waits before either half executes.
  Lower read dependencies currently use conservative lane masks.
- VU0 M points yield without clearing RUN or losing TPC. Both bounded start
  and continue runners preserve RUN at the host cap. The shared EE retirement
  and parked scheduler paths advance yielded VU0/VU1 and pending XGKICK.
  COP2 synchronizes before observing a running VU0. A runaway program stalls
  COP2 at the original instruction instead of fabricating a stop.
- GIF stream ownership persists until EOP across producer fragments. PSE,
  M3R, VIF1 MSKPATH3/M3P, and IMT PATH3 IMAGE interruption are implemented.
  A PATH3 IMAGE resumes after the interrupting path finishes. DIRECTHL waits
  for the active PATH3 packet. VIF retains DIRECT payload and trailing codes.
- GIF/VIF DMA advances MADR/QWC only for accepted quadwords. A suspended
  payload retains tag completion and IRQ decisions, CALL/RET stack state,
  MFIFO wrapping/producer availability, and REFS/STADR stalls. A stopped DMA
  clears its saved tag state. No caller-owned buffer is retained by GIF.
- CPU GIF writes use a serialized 16-quadword FIFO. A full FIFO declines a
  write; a guest SQ retries without advancing PC or retirement. GIF_STAT
  reports FQC, APATH, IP3 and path requests at their documented bit positions.
- A second blocked XGKICK cannot replace the first transfer. It retires no
  pair and retains its source pointer until arbitration allows continuation.
- EE/IOP register residency analyzes actual emitted word accesses instead of
  treating an MMI-containing block as a special fixed-width guest subset.
  All 128 architectural EE words, including upper lanes, are eligible for
  the 12 callee-saved host slots. Read intervals guide selection. Existing
  temporary allocation, alias handling, canonical writeback, opaque-helper
  fences and generation refresh remain in force.
- New timing/transport fields are appended. Old EE/GIF/VIF/DMA/VU1 checkpoint
  prefixes are zero-extended; the old field offsets are preserved.

## Recovered fixes

R1380–R1383 only changed Q status publication, synchronous GIF status reporting,
VU0 cap lifecycle, and added Q tests. Their Q tests are retained. Their useful
semantics are incorporated above, rather than overwriting the new scheduler
with the earlier partial implementation. GIF_STAT APATH uses bits 10–11;
P3Q/P2Q/P1Q use bits 6/7/8. See PCSX2 Gif.h and the bundled VUops.cpp
_vuFMACflush, _vuAddFMACStalls and _vuFDIVflush reference implementations.

## Build and validation

The DSI target uses the exact recovered official Docker SDK package layer:

- Image: devkitpro/devkitppc:20260503, linux/amd64.
- Manifest: sha256:4c919aa26151dd43d88ca28c922d1fe2409579a8ba60ef56517baf1abdfb1a48.
- Package layer: sha256:3727f0a7f966b9fae8b3cdbe70e976d2bd28a4c1409c525278ebd104577bc15a.
- devkitPPC r50 / GCC 16.1.0, libogc 3.1.0, matching modern runtime/libfat.
- The DSI target retains LIGHTREC, GX additive/fence, ARM beta and EE/IOP/VU
  AOT switches. GNU lightning's missing generated public header is rebuilt
  deterministically from its original template by tools/build_r1384.sh.

The binary uses the owner's recovered private AOT2 profile: 512 EE entries,
4,008 EE bodies, 256 IOP entries and 44/64 VU upper/lower entries. The published
source package contains empty generated tables, as R1379 did; build with
`bash tools/build_r1384.sh PROFILE_JSON` to regenerate from your private inputs.
The same profile's `.vu0.bin` and `.vu1.bin` companions are required.

Executable evidence is in validation/R1384: 20 focused native suites,
4,704 EE, 3,856 raw/timed VU and 255 IOP host-AOT comparisons, 2,400
generated PPC allocation cases, 97 linked allocation oracles and independent
linked FMAC/FSSET/FDIV plus hot timed-AOT arithmetic checks. The focused native suite covers
flag visibility, MAC lane masks, same-clock FSSET/FDIV, M continuation, host-cap
lifecycle, GIF ownership, IMT, DIRECTHL, DMA pause, REFS, CALL/RET, MFIFO, CPU
FIFO capacity/SQ retry, Q/P, XGKICK, checkpoint compatibility and prior COP2,
128-bit transfers and MMI tests. A generated-PPC oracle executes 2,400
full-128-bit allocator/residency cases with aliases and GPR zero preservation.
Linked ELF comparisons, DOL/ELF section equality and MEM1 bounds are separate
checks. None of these is a physical Wii boot, gameplay or FPS measurement.

## Material remaining limits

- Native VU blocks cannot bypass the new timing state. Those pipeline-blind
  blocks are conservatively disabled at the production entry classifier;
  compatible half handlers and AOT remain. Hot arithmetic words now have compiler-specialized native AOT datapaths
  returning their MAC/STATUS results to the common timed scheduler. Cold words
  retain a shared scalar fallback. A pipeline-aware dynamic block backend
  remains work, and this checkpoint may cost VU performance.
- Scalar FMAC arithmetic uses host float32 with range handling, not a proven
  bit-exact model of every PS2 mantissa/truncation, ACC intermediate overflow
  or EFU polynomial case. Lower dependency lane masks can over-stall.
- Existing CLIP timing and integer forwarding remain simplified. The global
  EE/IOP scheduler still uses instruction slots; M scheduling is not proof of
  exact EE/VU clock synchronization in every interlock/interrupt case.
- GIF SIGNAL double-signal stalls, GS readback direction, detailed physical
  bus/FIFO timing, and all VIF command stall combinations are not validated.
- Residency keeps canonical storage at helpers and uses word-level intervals;
  it does not implement a global whole-program 128-bit SSA allocator or all
  requested rematerialization/helper contracts.
- Physical Wii/Dolphin BIOS and Tekken validation is required. No FPS gain,
  full dynarec completion, complete hardware accuracy or playable game claim.
