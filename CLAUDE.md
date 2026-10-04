# Gekko2 development handoff — R1304

Start with README.md, STATUS.md, docs/ROADMAP.md, docs/R1301-HANDOFF.md and docs/R1304-HANDOFF.md. Historical round documents are evidence for their own checkpoints, not current promises.

The project is named Gekko2. Preserve legacy sd:/pcsx2/ data paths. Current builds, launcher and logs use Gekko2. R1304 integrates guarded direct-RAM byte/halfword/word operations in precise EE blocks. Keep the memory-specific live preparation callback and guard the fetched first memory instruction before advancing PC. Preserve R1303 constant exits and R1302 artwork. Every guest boundary remains precise.

Owner direction: pause GX feature expansion and prioritize our own PPC EE/IOP dynarec and stable BIOS/OSDSYS. Register allocation, larger guarded memory/control blocks and IOP blocks are next. Wii64/nullDC4Wii are references, not imported engines.

Preserve precise interrupts, Count/Compare, timers, delay slots, exceptions, TLB/source invalidation and SIF semantics. Conservative EE blocks retain per-instruction prepare/retirement checks. The older resident transform primitive is not the CPU execution loop. Keep reachable direct helper calls and far/unaligned fallback. Do not free executing native code.

Preserve CT24 destination alpha, GS VRAM ownership, overlap barriers, exact supported blending/depth rules and software/hybrid fallbacks. Maintain one opt-in GX setting. Read docs/GX-PRESENTATION.md and R1300-HANDOFF.md before editing rendering.

Run relevant native tests and generated/linked-PPC verification for code changes; host tests cannot run PPC machine code. Distinguish native tests, synthetic instruction counts and owner-observed hardware output. Never fabricate guest frames, force progress by overriding registers or claim full JIT/gameplay/FPS without evidence.

Never commit BIOS, discs, private RAM/checkpoints, configuration dumps, runtime logs, SDKs or generated builds. Preserve GPL and individual third-party notices. Public screenshots must be labeled native/host/Wii correctly.
