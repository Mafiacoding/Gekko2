Historical audit checkpoint. Superseded by R1379B.md for build status.

# Graphics audit checkpoint (based on R1378)

Implemented and checked:
- VIF UNPACK fill-mode payload length now counts consumed source vectors,
  excluding lookahead. Closed-form O(1) calculation replaces per-vector scan.
- Read live implemented VIF command registers via EE MMIO: MARK, CYCLE,
  MODE, MASK, CODE, ITOPS/ITOP, VIF1 BASE/OFST/TOPS/TOP, ROW/COL.
  STAT exposes the existing VIF1 DBF state. No invented NUM/ERR/FIFO behavior.
- Correct VU lower-op audit encoding: IOR is funct 0x35; 0x33 is reserved.

Validation:
- Payload calculation checked across 16,777,216 CL/WL/NUM combinations.
- 47 focused native regressions pass, including fill-followed-by-MARK,
  VIF register reads, VU, DMA, GS edges/DATE, CHD, controllers and audio.
- The separate older tests/test_vif.c still reports 8 failures. Some fixtures
  assume identity cycles without STCYCL and reuse dirty VU memory. This file
  needs further isolation and review; it is not reported as passing.
- No physical Wii, Dolphin rendering, PPC runtime or performance validation.
- This is a source checkpoint, not a tested DOL release.

Actual remaining implementation gaps found in source:
- VU XGKICK does not model cycle-metered asynchronous transfer/queue timing.
- VIF MSKPATH3 remains a no-op; GIF path priority needs coordinated DMA work.
- GS mipmaps omit automatic MTBA and trilinear blending.
- GX still uses hybrid CPU depth/readback paths for unsupported combinations;
  disabling tests or removing fences globally would sacrifice correctness.
- VIF MMIO NUM/ERR/FIFO state and STOP/force-break need a consistent model.
- CHD multi-track/parents/split changes from the lost scratch session are not
  in this recovered checkpoint; R1378 CHD limitations still apply.

References inspected on 2026-10-11:
https://github.com/PCSX2/pcsx2/blob/master/pcsx2/Vif_Unpack.cpp
https://github.com/PCSX2/pcsx2/blob/master/pcsx2/Vif.h
https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS/GSState.cpp
https://github.com/jpd002/Play-/blob/master/Source/ee/Vif.cpp
https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/VideoCommon/VertexManagerBase.cpp

PCSX2 bounds incomplete BIOS PATH1 packets. Our VU sender already supplies
one bounded memory-ring traversal, so copying that workaround without
reconciling GIF continuation state is not justified. Dolphin flushes batches
before capacity overflow; it is a GX consumer implementation, not a PS2 GS
replacement. Forum/search claims were not treated as code specifications.
