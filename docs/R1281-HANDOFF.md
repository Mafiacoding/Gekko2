# R1281 — BIOS-first CPU optimizations and measured profiling

## Real Wii evidence
User confirms R1280 Wii Remote now works perfectly. R1279/R1280 reached the Sony Computer Entertainment splash on real Wii; stable OSDSYS hardware menu remains unconfirmed. Uploaded logs show first image at 499273/501737 ms and EE295584532/295496452 respectively: R1280 did not materially improve hardware cold boot. Combined EE/IOP execution consumes about 94–98% of observed wall time; it does not establish the separate EE/IOP shares. Later guest VBlank rate is about 0.14/s. Real GS source remains 640x256, PAL output 640x576. BIOS/game work remains ahead of graphics polish and Tekken.

## Changes
- Cost-aware IOP dispatch keeps 19 cheap scalar forms inline while preserving their native translators for future blocks. Individual eight-instruction warm JIT tests improve about 1.4–5.3% in PPC instruction counts. Memory/control instructions retain their existing JIT paths.
- EE HBLNK timer uses exact 32-bit remainder while the bus counter fits, and the original 64-bit remainder otherwise. Active-divider helper count falls 87 to 53 PPC instructions. Timer retirement, clock periods, checkpoints and overflow semantics remain intact.
- Native EE MFC0/MTC0 transfers cover all 32 registers, signed extension, rt0, Config protected bits and Compare IP7 clearing. BC0/TLB/ERET still fall back. CPU dispatch deliberately uses cheaper inline C for single transfers; native transfers are available through the JIT API for future blocks. Complete block/register allocation JIT is still unfinished.
- Sampled host time-base profiling records separate EE/IOP execution costs. Sampling uses randomized intervals averaging roughly 256 slices; genuine 8 EE : 1 IOP order is unchanged. CPU_SAMPLE pairs, EE_tb, IOP_tb and percentages are approximate sampled wall costs, not exact cycles. Interrupt/background work can affect samples. EE_TIMERS reports actual modes for identifying active dividers.
- Toggleable FPS counter smooths up to twelve five-second samples, about 60 seconds. FPS counts emulated VBlank events; OUTPUT counts host presentations, including repeated images. Raw five-second log samples remain available. Existing Remote/Nunchuk controls and HBC exits remain.

## Validation and measurements
183 native regression tests pass. Both final Wii ELFs pass the inherited PPC suites plus sampled scheduler ordering/time-base wrap, FPS window rollover/guards, 44 divider/carry/wrap cases, and 1024 COP0 byte-oracle cases per build. Tests execute actual compiled PPC code with mocked allocation/cache services; they do not run on real Wii.

Controlled interleaved ADDIU/BEQ/NOP workload with HBLNK, 4096 EE and 512 IOP retirements:
- JIT warm: 2773814 -> 2632789 PPC instructions, about 5.08% fewer.
- Interpreter warm: 2681324 -> 2550016, about 4.90% fewer.
The JIT is still more expensive than the Interpreter in this synthetic workload. These numbers include sampling overhead and are neither hardware FPS nor a BIOS workload benchmark. COP0 full-retirement JIT samples improve MFC0 4697->4233, MTC0 Compare 4569->4105, Config 4545->4081; interpreter also benefits from timer/control-path compilation changes.

Native saved OSDSYS state advances from EE2054623152 to EE2057819183 without EE/IOP halt (PC0022e2cc / IOP00019418). This is a warm checkpoint continuation, not proof of a new cold boot or stable OSDSYS on Wii. Private state and BIOS config are packaged separately; public checkpoint excludes BIOS, discs and guest RAM.

## Installation and next hardware test
Replace sd:/apps/pcsx2-wii/boot.dol with PCSX2-Wii-R1281-Menu-JIT.dol renamed boot.dol. Retain BIOS, config and disc files. Interpreter DOL is included for comparison. SETTINGS 2 toggles FPS; HOME pauses; HOME+MINUS exits to HBC; launcher offers EXIT TO HBC. Debug log: sd:/pcsx2/R1281-boot.log. After a BIOS session, provide this log so CPU_SAMPLE identifies which CPU path should be optimized next. Do not promise 5/10 FPS or a completed JIT port.

## Deliverables
Checkpoint includes source, tools, docs, both ELF/DOL builds, verification reports/logs, hashes and cumulative Claude patch/diff against the originally uploaded project. Patch is dry-run checked, applied and byte-compared against every changed source file. ZIP integrity and manifest hashes are checked.
