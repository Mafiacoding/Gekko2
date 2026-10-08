# Gekko2 development handoff — R1341

Read docs/R1341-PERFORMANCE-ARM.md first. Then read docs/R1340-TEKKEN-STREAMING.md and docs/R1339-ARM-TEKKEN.md first. Then read docs/R1338-CACHE-ARM-IOS.md for recency/admission and persistent explicit IOS selection. Then read docs/R1337-DECODER-CACHE-ARM.md first for decoder, size-class arena and experimental MLOAD loader. Then read docs/R1336-VIDEO-CACHE-ARM.md for quadword/FIFO, exact code
publication, VDEC/PACK and experimental ARM IOS service changes. Then read docs/R1335-IPU-HLE-ARM.md for implemented IPU/HLE paths, cache A/B,
ARM protocol and explicit remaining MPEG/IOS/game limits. Then read docs/R1334-EE-CACHE-GAME-DIAGNOSTICS.md first, and docs/R1334-ARM-ROADMAP.md for future ARM work. Then read docs/R1333-COLD-BOOT-CACHE.md for session controls, unique logs,
boot resets and measured budget-variant cache behavior. Then read
docs/R1332-CACHE-PROFILER.md for counters/cache/IOP paths,
then docs/R1331-BATCHES.md for CPU/GX options, six batches (Fastmem is
batch 6), remaining work and verification limits. The owner authorizes CPU
and GX optimization behind independent options; Wii is the primary target.
R1330L-HANDOFF.md describes the inherited resident renderer baseline.

Start with README.md, STATUS.md, docs/ROADMAP.md, docs/R1301-HANDOFF.md and docs/R1304-HANDOFF.md and docs/R1305-HANDOFF.md, docs/R1306-HANDOFF.md and docs/DYNAREC-COMPLETION-PLAN.md. Historical round documents are evidence for their own checkpoints, not current promises.

The project is named Gekko2. Preserve legacy sd:/pcsx2/ data paths. Current builds, launcher and logs use Gekko2. R1306 integrates direct/live TLB RAM through quadword/FPR transfers and selected block-terminal controls. A regular branch always marks its delay slot, taken or not; keep the shared emitter fix and scalar delay-slot path. The memory preparation callback returns physical offset+1 (zero declines); the native memory variant takes a third fetched-first physical-proof argument. Resolve every instruction, exclude virtual MMIO/scratch, and guard the fetched first memory instruction before advancing PC. Preserve R1303 constant exits and R1302 artwork. Every guest boundary remains precise.

Keep PPC EE/IOP dynarec and BIOS/OSDSYS behaviour stable while developing the
resident GX path. Wii64/nullDC4Wii are references, not imported engines.

Preserve precise interrupts, Count/Compare, timers, delay slots, exceptions, TLB/source invalidation and SIF semantics. Conservative EE blocks retain per-instruction prepare/retirement checks. The older resident transform primitive is not the CPU execution loop. Keep reachable direct helper calls and far/unaligned fallback. Do not free executing native code.

Preserve CT24 destination alpha, GS VRAM ownership, overlap barriers, exact supported blending/depth rules and software/hybrid fallbacks. Maintain the main opt-in GX setting and independent optimization options. Read docs/GX-PRESENTATION.md and R1300-HANDOFF.md before editing rendering.

Run relevant native tests and generated/linked-PPC verification for code changes; host tests cannot run PPC machine code. Distinguish native tests, synthetic instruction counts and owner-observed hardware output. Never fabricate guest frames, force progress by overriding registers or claim full JIT/gameplay/FPS without evidence.

Never commit BIOS, discs, private RAM/checkpoints, configuration dumps, runtime logs, SDKs or generated builds. Preserve GPL and individual third-party notices. Public screenshots must be labeled native/host/Wii correctly.
