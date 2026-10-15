# R1376B Wii validation incorporated into R1377

User reports the previous freeze is resolved. This log supports a 32-minute run, PAUSED and clean EXIT_HBC at 909,706,623 EE instructions. This is evidence for this run, not a universal stability guarantee.

R1377B already includes every R1376B sparse-seeding/fence/readback fix. Executables remain the tested R1377 builds; this update adds measured evidence, not speculative speed hacks.

- Whole log: 1940.095s; guest VBlank 0.134208 Hz; EE retired 468897/s.
- First 20 intervals: 100.499s; guest VBlank 0.258266 Hz; EE retired 1264807/s.
- Last 20 complete intervals: 105.702s; guest VBlank 0.160385 Hz; EE retired 21503/s.

EE retired/s is not an independent speed measure when CPU idle skipping changes instruction retirement. Guest VBlank advances about 0.16 Hz in the tail despite EE retirement dropping strongly. Compare identical boot phases rather than these whole-run averages against a shorter older log.

Tail 20 complete random CPU sampling intervals: EE 36.12%, IOP 12.06%, scheduler 2.54%, idle 18.36%, compile 2.68%, GS raster 13.74%, GX readback 14.46%. These are sampling shares, not hardware utilization. No RAM/GPU utilization percentage follows from them.

GX accepted 86,701 textured Gouraud triangles through CPU shading + resident GX blend, not fully hardware-shaded triangles. 134,984 visible Gouraud triangles remained software, 133,566 classified texture fallback. That label alone does not reveal whether changing-Z, texture alpha, blend, or another hybrid admission condition caused rejection. It is not evidence that all textures are unsupported.

Sparse seed uploaded 540,200,000 bytes, reused 24,450,455 tiles. Full-surface seeding must remain disabled. Bounded readback reports 6,991,143,808 bytes avoided versus full-target copies. These optimizations remain enabled in R1377B.

JIT code arena MEM1 6 MiB, used 5,925,248 bytes, high-water 6,035,680, allocation failures zero. Fragmentation is visible (largest free 7,968; 647 holes), but this log does not establish a leak. Global flushing would risk destroying hot blocks and repeating expensive compilation.

DSI: 80 faults, 80 patched; 109,163 emitted sites. More faults are not a goal. Lightrec ready but only MULT/MULTU/DIV/DIVU pilot, not a full EE or IOP replacement.

ARM IOS222 loaded an older worker CRC 567f1f55 with capability mask 1; zero submitted/completed jobs. ARM_RAM declines capability; ARM is not taking CPU work off the EE in this log. Do not report ARM acceleration from availability alone.

R1377 adds proven shared-edge coverage correction and GS_SOURCE_RGB source-color telemetry. No new timing ratios, unsafe cache flushes, skipped VU commands or brightness hacks are justified by this log. Test R1377B from cold boot with identical settings and compare BIOS geometry/darkness plus the new source RGB line.
