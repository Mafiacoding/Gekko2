# R1259 PADMAN corrections / resumed work

R1258 was restored from its validated 8,700,747-byte checkpoint after the
execution workspace reverted to R1253. Runtime logs from the previous
R1259 coldboot did not survive; the fresh repeated test below confirms the main menu.

Corrections reproduced from the prior investigation:
- OSDSYS 0x208920 calls padGetConnection (0x257430) before padPortOpen
  (0x256490). Publish physical connection status before any port opens.
- BIOS XPADMAN 3.06 text+0x604 stores button copy function return into
  DMA offset 96. Valid button copy returns 32 at text+0x3ef8.
  OSDSYS padRead 0x256858 returns it; 0x208a88 requires 32 bytes.
  Change normalized data[32] length 4 -> 32, preserve raw SIO2 layout.
- test_pad_new_dma now covers pre-open connection bits, unplug, both
  buffers and normalized length.

Performance assessment: EE JIT currently executes one translated EE
instruction per call, returning to ee_step housekeeping each time.
PC L0 cache reduces dispatch but does not form basic blocks. Priorities:
1. Measure EE / IOP / VU / renderer host time and JIT coverage.
2. Basic-block EE execution with precise guest interrupt/event boundaries.
3. Retain registers within blocks; direct branches and block linking.
4. Fast RAM access with TLB, MMIO and code invalidation guards.
5. IOP and VU acceleration and measured rasterizer bottlenecks.
No claimed speedup. 0.08 -> 5 FPS requires 62.5x overall acceleration;
EE-only JIT cannot guarantee it. No Dolphin runtime available here.

Priority agreed with user: finish OSDSYS Browser and System Configuration,
verify graphics/input/settings, optimize measured OSDSYS performance, then Tekken.
A bounded fresh-input driver with Count/PC/idle heartbeats is included in
tools/verify_osdsys_setup_r1259.c. It changes no guest state except real pad buttons.

## Confirmed fresh OSDSYS result

Completed fresh BIOS Interpreter boot with 50,000 interleaved slices per
call and only actual controller CROSS press/release:
- XPADMAN GETVERSION/INIT at EE 50,373,465 / 50,375,211.
- Real padPortOpen at EE 127,081,544, port 0/slot 0, area 0x002fd000.
- CROSS sequence advances initial prompt -> Language -> Time Zone ->
  Daylight Savings Time -> final notice -> Browser / System Configuration.
- Final EE=1,955,407,338, PC=0x0022ea84, HALT=0.
- Main menu frame contains Browser, System Configuration, X Enter,
  triangle Version. No guest PC, completion flag or RAM edits.
- Display proof samples actual GS DISPFB2/DISPLAY2 region (PMODE=0x66,
  SMODE2=3, framebuffer width 640, active region 640x256), scaled to
  640x448 using the same source-coordinate mapping as the Wii output.
  Older raw 640x448 dumps also include pixels outside the active region.
- Both PPC builds passed 96 helper checks each; 165 native regressions pass.

Open work: native menu proof is not a Dolphin test. Blue orb rendering is
visibly incomplete. NVRAM/settings persistence is still a placeholder;
Back, empty Browser, further settings edits and reset persistence need input validation.
VU1 snapshot: 1,906 instructions, 48 unimplemented pairs, seven PATH1
transfers. These counters alone do not identify the orb rendering cause.
Do not claim a measured FPS gain or fully correct OSDSYS yet.

## System Configuration continuation

A diagnostic continuation reaches the actual System Configuration screen
with current date/time and Language / English, after normal DOWN, CROSS,
CIRCLE, UP input. Final EE=2,259,231,884, HALT=0.
This continuation is not another fresh boot: the old checkpoint does not
serialize host PAD DMA bindings. The diagnostic restores only the successful
INIT/OPEN host-service bindings from the recorded trace, after validating
the guest's real opened-port structure. It does not force guest RAM/PC/registers.
The BIOS-specific driver is included for review; generic checkpoint binding
serialization remains open. The short input spacing did not establish Back
or the empty Browser screen; those still need deliberate timing checks.

Graphics follow-up: final TEX1 context 1 MMAG=1/MMIN=1 requests linear
filtering, whereas the software sampler currently uses nearest texels.
Triangle STQ interpolation also warrants auditing: the current formula
interpolates inverse Q and S/Q, rather than verifying the GS homogeneous
S/T/Q convention. No unverified renderer candidate is in this release.
