# Gekko2 handoff — R1354 Beta

Read docs/R1354-SIF-BARRIERS.md first. Claim SIF registers before synchronization; keep all actual shared access ordering. SYNC_REASONS counts barrier calls, not ticks. CPU_SAMPLE now tracks EE-only tails. Synthetic INTC-read cost drops6.7%; RAM-store cost unchanged. No physical Wii R1354 speed/boot claim before user logs. Same worker, Cold Boot. New IOP catch-up blocks and adaptive cache are OFF by default, stored in version8 settings. Registered sysclib RAM HLE is optional, bounded, with verified ARM-data fallback; not full BIOS HLE. Build tools/build_r1354.sh.

# Gekko2 handoff — R1353 Beta

Read docs/R1353-EVENT-SCHEDULER.md first. R1352 Wii log29 confirmed ARM handshake but no CSC jobs and first image418915ms. R1353 changes instruction-slot interleave behind independent OFF-by-default switches; not a cycle-accurate PCSX2 scheduler. Keep SIF/DMA synchronization, urgent-event notifications, source/mapping generation invalidation and pinned ownership. Native EE windows can reach32 instructions. IOP catch-up deliberately retains warm one-tick native owners; parked idle advances only before known deadlines. Optional SIO2 ABI HLE registers real loaded exports and retains original code for unsupported/changed calls. No twofold Wii speed/geometry/full-IOP/full-JIT claim. Existing host checkpoint is NOT a Wii quick-boot feature. Build tools/build_r1353.sh. Current worker wire ABI remains R1352-compatible.

# Gekko2 handoff — R1352 Beta

Read docs/R1352-EE-ARM-VU.md first. Source and executable tests distinguish implementation from Wii verification. R1351C first image415557ms; DSI worked but boot was not faster. ARM old magic00000031 remained rejected. Log path=1 is real PATH2, not VU1. No four-minute boot/full-JIT/geometry claim. Build tools/build_r1352.sh. Install the new ARM ELF and fully restart Wii before checking the handshake.

# Gekko2 development handoff — R1351C

Read docs/R1351C-DSI-BLOCKS.md first. Wii B self-test succeeded but emitted0
sites. C integrates DSI into the actual precise LW block path; event windows
retain nonfaulting RAM proof. Build tools/build_r1351c.sh. Historical A/B
builds must come from their frozen package, not rebuilt from the C tree.

Read docs/R1351-TOOLCHAIN-DSI.md first. R1351A and B use devkitPPC r50,
GCC16.1.0/libogc3.1.0. B alone adds the self-tested, kernel-segment EE LW
MMU/DSI pilot. The paired build script is tools/build_r1351.sh. Linked PPC
tests pass with modeled privileged hardware; physical Wii boot/speed for
R1351 is pending. R1350 Wii log24 still has corrupted ARM capability data
and identical zero vertices. Do not report those problems fixed or promise
a four-minute boot. R1350's first image took412015ms. Full JIT remains partial.

Read docs/R1342-PERFORMANCE-CPU-ARM.md first. Then docs/R1341-PERFORMANCE-ARM.md. Then read docs/R1340-TEKKEN-STREAMING.md and docs/R1339-ARM-TEKKEN.md first. Then read docs/R1338-CACHE-ARM-IOS.md for recency/admission and persistent explicit IOS selection. Then read docs/R1337-DECODER-CACHE-ARM.md first for decoder, size-class arena and experimental MLOAD loader. Then read docs/R1336-VIDEO-CACHE-ARM.md for quadword/FIFO, exact code
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
