# Gekko2 SPEED BETA12.20 – 15 October 2026

**Aktueller Stand:** R1389-SPEED-BETA12.20-GX-ALPHA-FENCES. Quellcode und Host-Regressionen; Hardware-Gameplaytest steht noch aus. Persistente 128-Bit-PPC-Registerübergabe über unabhängige JIT-Blöcke und vollständige GS/GX-Hardwaretreue bleiben offen. Die darunter folgenden früheren Statuspunkte dokumentieren ältere Versionen.

## Historischer Stand: BETA12.9

**Wii compile passed; physical Wii test not yet performed.**

- New PS2 GS `TRXDIR=2` local-memory copy support, same-PXM layouts only,
  overlap-aware, no synthetic frame flip, host source and GIF-register tests PASS.
- Expanded generic VBlank matcher for register-held MMIO base, with checked
  side-effect/delay-slot, MMIO address and real IRQ guards; host matcher PASS.
- Current preceding Wii result: Crash E3 font fixed, first image on screen,
  roughly 2 emulated guest VBlanks/s in some intervals, later VSync wait
  continues while displayed buffer is black and alternate draw target nonblack.
- This new GS move implementation is a *candidate* for missing transfers;
  no evidence yet that the Crash demo actually requests `TRXDIR=2`.
- No supported claim of full BIOS speedup or in-game success.
- See `docs/BETA12.9-GS-TRANSFER-RESEARCH.md` and older history below.

---

# Gekko2 — BETA12.5 FILEIO TRACE

The owner's second BETA12.4 Wii log positively contains:
`CRASH_E3_NUERROR pc=00103d74 caller=0012fa18 provider=30313132`
`heading="NuError - nufont.c(117) : " detail="unable to create font fonts\ariel32"`.
The emulator's FILEIO provider is genuinely `2110` (little-endian ASCII bytes), but
font creation fails, and GS display remains disabled (`PMODE=0`). Zero ISO-cache
transport failures are reported. This does not prove FIO read delivery is correct.

BETA12.5 adds diagnostic counters only: FILEIO provider query; OPEN attempts and
status, READ count/actual bytes, CLOSE operations; plus counters for paths containing
`ariel32`. The existing file RPC responses and game state are unchanged.
Build is verified by Wii cross-compilation, DOL/ELF validation and MEM1 checks.
**The new diagnostic DOL has not yet been run on Wii. No title screen claim.**

---

# Gekko2 — SPEED BETA12.4 ERROR DIAG status

**Built, not physically re-tested:** Direct disassembly of the private Crash E3
ELF proves that `0x00103D70` executes an unconditional self-branch inside the
NuError path. GX and Software Wii logs both end with `FIRST_IMAGE=0`, `PMODE=0`
and a stationary guest CPU at this address. The approximately 46-minute Software
run retained the same fatal loop; the ISO cache recorded 0 failures.

BETA12.4 adds only a read-only fatal-error probe after the existing BETA12.3
legacy FILEIO correction. It emits a single `CRASH_E3_NUERROR` record with the
formatted error and caller if both the self-branch and original Nu2 data
fingerprint match. No game state or HLE responses are changed.

**Not established:** Actual fatal-message text on Wii, successful Crash title,
GT3 race or Tekken gameplay, or performance increase. Wii build and probe native
test succeeded, but the new DOL has not been run on Wii/Dolphin.

See [BETA12.4 verification](docs/BETA12.4-CRASH-E3-NUERROR.md).

---

# Gekko2 — SPEED BETA12.3 FILEIO TEST status

**Host-verified:** real Crash E3 `FONTS/ARIEL32.FNT;1` can be opened through
legacy and modern FILEIO OPEN layouts, plus lowercase path without ISO version
suffix. Version strings 2050 and 2110 are parsed from the selected IOP FILEIO
module, not supplied by a per-game success stub. ELF/ISO, basic VU and
RPC tests pass. Wii devkitPPC cross-build and DOL structural checks pass.

**Unverified:** Crash E3 title on Wii/Dolphin, New Game and gameplay; the
VU integer-to-conditional-branch hazard investigated by Codex; GT3 memory
card/race, Tekken post-VS/VIF graphics, and Wii FPS improvement.

See [BETA12.3 verification](docs/BETA12.3-CRASH-E3-FILEIO.md).

---

## R1387: independent GIF / GS Q

Preserves R1386 AOT and modern DSI toolchain. Corrects Q latching and
checkpoint compatibility. The reported BIOS/PCSX2 visual discrepancy
remains unresolved: ten controlled BIOS captures are unchanged.
See [implementation, tests and exact limits](docs/R1387-GS-Q.md).

## R1386: guarded AOT windows and IRQ/checkpoint fixes

Modern DSI build with devkitPPC r50 / GCC 16.1.0 / libogc 3.1.0.
Preserves R1384 accuracy and R1385 timed VU hybrid JIT. Adds cached AOT
lookup, guarded arithmetic windows, idle graphics service checks, safe
IRQ wakeup/preemption, VU1 cold reset and negotiated pad checkpoint state.
See [fix inventory and remaining limits](docs/R1386-FIX-ABGLEICH.md).
PPC instruction reductions are measured in a modeled linked binary;
physical Wii speed, BIOS crash and correct Tekken/GT3 gameplay remain open.

## R1385: timed VU hybrid blocks

VU0/VU1 production paths use dynamically emitted PPC blocks with precise
per-pair timing and source guards. Native fused non-arithmetic pairs are
available inside the timed scheduler. See [R1385 details](docs/R1385-TIMED-HYBRID-JIT.md)
for executable validation and remaining accuracy/gameplay limits.

# R1384 Codex checkpoint

R1384 adds executable FMAC timing, VU0 M continuation, resumable GIF/VIF DMA,
a CPU GIF FIFO, and general word residency across all 128 EE GPR words.
The DSI build uses the recovered devkitPPC r50 / libogc 3.1.0 Docker SDK and
the owner's AOT profile. See [R1384 implementation and limits](docs/R1384-CODEX-HARDWARE.md).
Complete hardware accuracy and a fully native timed VU backend remain unproven;
physical Wii boot/game/FPS checks are pending. Earlier sections below are
historical checkpoint evidence.

# R1379-AOT3 — AOT2 continuation and PCSX2/GIF fixes

The working AOT2 workflow is retained with guarded EE delay slots, BC2,
LQC2 VF0 reads, correct J/JAL regions and metered VU1 XGKICK transfers.
The R1379B incomplete-PATH1 fix is included. Existing graphics/VIF/CHD
corrections remain present. The owner reports approximately halved BIOS
boot time and nearly 0.30 FPS in Tekken for AOT2; AOT3 hardware performance
has not yet been measured. Full dynarec and PS2 hardware fidelity remain
in development. See [changes, reproduction, evidence and remaining gaps](docs/R1379-AOT3.md).

# R1378-AOT2 — hybrid static compilation checkpoint

EE scalar/complex/memory, IOP tick-safe and VU0/VU1 pipeline-safe static paths
are integrated. 4,500 EE + 1,928 VU + 255 IOP host comparisons, 37 existing
regression binaries and 110 linked-PPC comparisons pass. The private test
build contains 512 EE entries, 256 IOP entries and 44/64 VU upper/lower words.
The MEM1 gate passes with 2,108,352 bytes remaining. This does not certify
full dynarec/hardware completion, physical Wii boot or faster FPS.
See [implemented scope and remaining work](docs/R1378-AOT2.md).

# R1352 Beta — dependent EE windows, word-safe ARM IPC, VU/FIFO mapping

See [implementation and limits](docs/R1352-EE-ARM-VU.md). Linked comparisons preserve exact CPU/timer/IRQ state. Dependent RAM-window microbenchmarks reduce PPC instruction counts by18–22%; actual Wii boot time remains unmeasured. VU local windows and CPU quadword FIFO stores are now connected. BIOS null-vertex repair is unverified; path=1 is zero-based PATH2. ARM transport uses aligned shared-word accesses and private staging; test the newly supplied worker after a full Wii restart.

# R1342 — EE prefix reuse, CPU guards and IOP deadlines

Four independently selectable optimizations reduce short EE grant overhead,
inactive CPU service calls, repeated THREADMAN scans and historical diagnostic
work. MLOAD status 1 is supported with complete-buffer/readback verification.
See [implementation, source audit, measurements and limits](docs/R1342-PERFORMANCE-CPU-ARM.md).
Warm short-grant tests improve by 21%; a mixed EE/IOP replay improves by 0.7%.
These synthetic PPC instruction counts do not establish doubled Wii FPS.
The four CPU options default ON and require cold boot. A separate default-OFF
experimental ARM option delegates large IOP BIOS RAM jobs to MLOAD and verifies
every byte. ARM/IOS changes require an app restart. Physical R1342 boot, ARM
handshake and a RAM-job speedup remain unverified.

# R1341 — cache/retirement and ARM transport test build

Guarded EE lookup/admission hints, reduced duplicate IRQ/IOP fetch work,
event-safe PPC timer bookkeeping and verified MLOAD status-zero transfers.
Short host button presses persist until the guest controller samples them.
See [implementation, measured tests and limits](docs/R1341-PERFORMANCE-ARM.md).
Targeted synthetic EE replay uses 6.07% fewer PPC instructions; a mixed replay
is essentially unchanged (+0.10%). **A 2x/3x Wii speedup is not established.**
ARM startup still requires a physical Wii test; its worker currently handles
CSC only. R1340 Tekken streaming stays intact. PLUS pauses; MINUS+A sends
PS2 START. No new setting is required.

# R1340 — retail sound/streaming transport

Verified-provider RSPU2 initialization/status, bounded disc-to-EE video reads,
separate 7/128 audio/video group extraction, sound-parameter storage and
checkpoint persistence. A private native cold boot now renders the **Tekken
Tag Tournament title graphic** in the real GS viewport. Interactive title
navigation, a complete FMV and streamed audio playback remain unverified;
physical Wii performance/compatibility requires a new test. See
[R1340 implementation and limits](docs/R1340-TEKKEN-STREAMING.md).

R1339 ARM loader bounds/diagnostics and R1338 cache/IOS settings are retained.
The owner's R1338 log measured FIRST_IMAGE 509303 ms vs 552449 ms; ARM completed
zero jobs. Those measurements do not establish a new R1340 speedup.

## Historical checkpoints

R1338: EE recency / budget admission, persistent ARM settings and explicit
CURRENT / IOS222 startup selection. See [R1338 details](docs/R1338-CACHE-ARM-IOS.md)
and packaged verification results; physical Wii speed/ARM loading unverified.

# R1337 status

BDEC/IDEC scalar coefficient decode and IDCT implemented; synthetic native and
linked PPC checks pass. Executable arena uses size bins and boundary tags.
Experimental MLOAD RAM loader can start the included ARM CSC service on a
compatible current IOS; standard IOS remains CPU fallback. No physical-Wii
ARM acceleration or complete game/movie boot is verified.
See [R1337 evidence and limits](docs/R1337-DECODER-CACHE-ARM.md).

# Historical Gekko2 status — R1335

Updated 2026-10-08. **Early alpha; alpha coming soon, without a fixed date.** This is the current project status. Historical round documents describe their own revisions and do not override it.

## R1335 IPU / HLE / cache comparison

Real input/output FIFO data and streaming CSC, FDEC, SETIQ/SETVQ/SETTH are wired
to EE MMIO and bounded resumable DMA. MPEG decode/PACK remain open. Known HLE
RAM copy/fill operations have a tested fast path; strict unknown-call fallback
is optional. Compact reuse is an independent 1024/4096-owner cold-boot switch.
ARM transport and worker handler are tested with mocked IPC; an IOS server and
ARM hardware timing are still needed. No measured Wii FPS gain or Tekken title
boot is asserted. See [R1335 evidence](docs/R1335-IPU-HLE-ARM.md).

## R1334 cache and game-diagnostic checkpoint

Block cache reuse ON now retains 4096 EE block owners; OFF retains 256.
The executable arena stays 6 MiB. Missing boot-local IOP masks/counters reset.
IPU_STATUS/IPU_COMMANDS expose the existing decoder gap during game tests;
MPEG output remains unimplemented. ARM offload is future work.
Validation: 26 linked PPC suites, focused CPU/IOP/IPU/log checks, and both Wii cross-builds pass.
See [R1334 handoff](docs/R1334-EE-CACHE-GAME-DIAGNOSTICS.md) and
[ARM measurement plan](docs/R1334-ARM-ROADMAP.md). Synthetic cache replay
improvements are not measured Wii FPS gains.

## R1333 session and cache changes

Cold Boot restarts the selected BIOS/disc mode and reserves a separate numbered
log. START/PLUS only pauses/resumes an existing session; requested CPU/GX options
apply on a cold boot. EE boot one-shots/SIF system registers and selected render,
translation and Fastmem counters reset. GPU ownership must resolve before a
restart frees old guest RAM. Hardware FIFO buffers remain allocated.

EE cache reuse now preserves same-PC block-length variants and selects by the
current retirement budget. A synthetic alternating 8/2 replay reduces modeled
PPC instructions 145,993→119,273 with identical guest results. Source/mapping
invalidation and precise EE8/IOP1 boundaries remain.

Validation: 25/25 linked PPC suites, 64/64 native GS tests, three same-process
cold boots, unique log/resume/write-error tests, both cross-builds and DOL/ELF
structure checks. Launcher image is a host render. No physical Wii R1333 boot
or FPS gain is certified. See [R1333 handoff](docs/R1333-COLD-BOOT-CACHE.md).

Owner-supplied R1332 Wii logs show Tekken first image near 64 million EE
instructions and accepted native GX draws, with substantial hybrid depth work.
The supplied fresh BIOS run ends near 203 million EE instructions before first
image. These are historical observations, not proof that R1333 fixes BIOS
progress or reaches playable performance. Earlier status sections below retain
their original checkpoint scope.

## Verified progress

Development branch work after R1308: VU nested control/link corrections,
shared FBRST D/T traps and event-free IOP timer intervals pass 214 host tests
and linked PPC oracles. See [R1309 working notes](docs/R1309-WORKING-NOTES.md)
for exact coverage and measured cost, including the dense-event regression.
This is source development; points 4 and 5 are not complete and no R1309
hardware release is certified.

| Area | Evidence | Remaining limits |
| --- | --- | --- |
| Real Wii BIOS | Owner confirmed the Sony Computer Entertainment startup screen and Wii Remote idle fix. Both earlier GX variants booted in owner testing. | Stable, responsive OSDSYS navigation and current-build FPS need hardware confirmation. |
| Native OSDSYS | BIOS menu renders; Browser entry and return were tested in native checkpoint work. | A native framebuffer capture is not evidence of current Wii speed or stability. |
| Tekken Tag Tournament | Namco logo reproduced in native tests and reported on Wii by the owner. R1297 retail SLUS-20001 resource-transfer fix passes the previous TLBL failure in fresh native boot; fractional sprite gaps are fixed. | Title screen/gameplay and playable performance are not demonstrated. The custom module profile is guarded, not a universal sound-module implementation. |
| Launcher/input | SD ISO/BIN browser, FPS/HUD options, Remote/Nunchuk/GameCube controls and HBC exit actions are implemented. | Nunchuk directions are digital; CHD launcher support is absent. |
| PPC JIT | Precise EE/IOP blocks, fused legal delay slots, RAM merge/vector paths, native returned-block continuation, IOP load/exception precision and guarded scalar register residency. | Full EE/VU coverage, wider allocation, patched generation-based links and event batching remain incomplete; Wii stability/FPS require testing. |
| GX | Optional CT32/24 presentation, guarded sprites/flat triangles, snapshot textures, selected TEV blending/depth paths, deferred VRAM resolve and compatible resident framebuffers. | Varying Gouraud/textured triangle routing, broad GPU residency and complex GS states still require software/hybrid handling. |
| R1302 branding | Gekko2 launcher header, HBC icon, checked 640×480 native launcher preview and both Wii cross-builds. | No fresh physical-Wii R1302 timing result. Guest emulation remains R1301. |

## R1308 VU pipeline and timer event checkpoint

Implemented Q DIV/SQRT/RSQRT lane selectors, 7/7/13-cycle issue latencies, WAITQ-before-pair stalls and E-bit flush. VU1 implements all 13 scalar EFU operations with P issue timing, WAITP and MFP; transcendental results follow the PCSX2 polynomial reference and are not claimed hardware-bit-exact. Lower FC/FS/FM instructions read canonical flags; full FMAC flag generation and delayed visibility remain open. Upper/lower VF hazards and I-literal ordering are corrected in both scalar and admitted native paths. Conflicting native pairs decline transactionally. Wrapped XGKICK packets use the existing synchronous PATH1 parser with EOP bounds.

EE peripheral timers defer only event-free intervals. Counts materialize on MMIO, diagnostics and checkpoints. Compare/overflow ticks use the original scalar transition; legacy mutable state falls back to scalar ticking. Count/Compare, CPU interrupt checks, EE8/IOP1 ordering, SIF and GS event scheduling remain per-instruction.

Measured timer-only PPC instruction cost decreases 73–79% across four clock sources. Complete warm synthetic EE eight-instruction samples decrease roughly 5–6%; IOP samples are unchanged. These are linked PPC instruction counts, **not actual Wii FPS measurements**. See [R1308-HANDOFF.md](docs/R1308-HANDOFF.md) for test results and remaining scope.

## R1307 register residency and IOP precision checkpoint

R1307 combines the development work after R1306: bounded IOP native blocks, exact EE8/IOP1 interleave, IOP delayed loads/merge forwarding and EPC/BD/TAR/alignment/overflow precision, extended EE control/merge/vector/COP1 paths, native returned-block continuation and ABI fixes. Reused scalar operands now stay in nonvolatile PPC registers across audited prepare/retire callbacks. Guest memory stays canonical; generation changes reload copies, and opaque bodies fence them. Memory-only and unsuitable blocks keep their earlier path.

Validation includes **211/211 host tests**, 62 independent helper-observation/generation oracles, 48 hostile resident-pool ABI cases, 245 wrapper cases, 477 paired IOP programs, 50 pipeline oracles, 1,152 merge byte oracles/64 IRQ boundaries, native-chain comparisons and paired COP1/link signatures. Detailed scope, slower as well as faster synthetic counts, install instructions and remaining work are in [R1307-HANDOFF.md](docs/R1307-HANDOFF.md). This is an early-alpha development checkpoint with requested test binaries, not a completed dynarec or a demonstrated Wii FPS improvement.

## R1306 wide memory and terminal-control milestone

LD/SD, LQ/SQ and LWC1/SWC1 join live-proven direct/TLB RAM blocks. Quadword effective addresses round down to 16 bytes; LQ $zero performs no access. Explicit load/store classification preserves permissions. Raw FPR bits, fpr[0], 64-bit loads and all 128-bit lanes are preserved.

Sixteen selected branch/jump families may terminate a precise block. Delay slots remain in the scalar frontend. Differential tests exposed and corrected the shared native regular-branch delay flag: not-taken ordinary branches still have a delay slot. This also fixes the existing scalar REGIMM not-taken gap. Likely-annul behavior is unchanged.

Validation: **203/203 native tests**, four linked-PPC jobs, 72 wide-memory programs, 128 controls, 48 wide-memory IRQ boundaries and 66 matching VRAM signatures. New wide-memory workloads count 22–41% fewer PPC instructions; existing TLB-LW costs about 1% more. These are synthetic platform instruction counts, not Wii timing. Details are in [the R1306 handoff](docs/R1306-HANDOFF.md) and [verification](docs/verification/R1306.json). A complete dynarec is **not finished**: general allocation, linked blocks, broad IOP blocks and remaining VU/EE cases are open. [The completion plan](docs/DYNAREC-COMPLETION-PLAN.md) lists implementation and readiness gates. No fresh physical-Wii boot or FPS measurement is claimed.

## R1305 live mapped-memory milestone

The eight admitted byte/halfword/word families now access proven physical RAM directly in emitted PPC code, including TLB-mapped KUSEG, KSEG2 and kernel-mirror addresses. Every instruction resolves its current mapping; ASID/global, even/odd page selection, V/D admission, pointer, alignment and bounds are checked before guest preparation. The fetched first instruction uses the same live proof before PC advances. Virtual MMIO/scratch and unsupported mappings remain scalar. This is conservative JIT admission, not a change to the scalar TLB fault model.

Validation: **202/202 native tests**, both cross-builds and four linked-PPC jobs. Ninety-six mapped programs match R1304 JIT and R1305 interpreter/JIT complete register/RAM signatures. Additional tests replace and revoke live mappings, overwrite source through a mapped alias, and interrupt mapped stores at every boundary. Inherited memory/ALU/COP1 checks and 66 VRAM signatures remain matched.

Matched warm eight-load PPC counts: mapped RAM **6458→4278** (33.8% fewer), direct RAM **3907→3766** (3.6% fewer). ALU **3180→3185**, mixed COP1 **3166→3171** (five extra setup instructions per eight-instruction block). These are linked PPC instruction counts under mocked platform services, not Wii cycles, BIOS timing or FPS. No fresh physical-Wii boot is claimed. [Handoff](docs/R1305-HANDOFF.md), [verification](docs/verification/R1305.json).

## R1304 memory-block milestone

Eight byte/halfword/word memory families (LB/LBU/LH/LHU/LW/SB/SH/SW) now run inside conservative EE blocks with ALU/COP1 instructions. Live KSEG0/KSEG1 main-RAM proofs check pointer, bounds and alignment before preparation. MMIO, ROM, scratchpad, TLB data and unaligned/out-of-range addresses decline into the existing scalar path before that instruction executes. Per-instruction source/mapping, retirement and IRQ checks remain. Memory instructions use a specialized preparation callback; ALU instructions retain their original callback.

Validation: **201/201 native tests**, paired cross-builds, four linked-PPC jobs, 64 mixed programs, 58 first-address decline cases, eight later-decline families, store-to-next-code mutation, all eight store interrupt positions, all 35 inherited source exits and nonvolatile-register checks. Complete memory/ALU/COP1 signatures match Interpreter/JIT; the memory signature also matches the old R1303 JIT. All 66 VRAM signatures agree.

Eight warm direct-RAM LW instructions count **5009→3907 PPC instructions** (about 22% fewer). Eight ALU/COP1 instructions add three PPC instructions (3177→3180 / 3163→3166). Allocation/cache services are synthetic; these numbers are not physical Wii FPS or end-to-end BIOS speed. TLB-mapped data is deliberately not accelerated by this new block path.

See [R1304 handoff](docs/R1304-HANDOFF.md), [nullDC4Wii review](docs/NULLDC4WII-REVIEW-R1304.md) and [verification](docs/verification/R1304.json). Full register allocation, control-flow linking, wide memory operations and IOP block execution remain unfinished. GX is unchanged.

## R1303 CPU change

Precise EE blocks return a constant retirement count at each exit rather than maintaining a counter in PPC r15. Zero tests use immediate comparisons. Every live source/mapping check, guest retirement, interrupt and timer boundary is retained. Source-exit stubs slightly increase generated code size; they avoid bookkeeping in the common full-block path. This is an incremental improvement, not a complete dynarec.

## Latest changes

- **R1297:** guarded Tekken retail resource transfers, fractional sprite coverage and precise COP1 block admission. [Details](docs/R1297-HANDOFF.md).
- **R1298–R1299:** snapshot sprites/flat triangles, exact scoped TEV blend cases, PABE specialization and split depth handling. [R1298](docs/R1298-HANDOFF.md), [R1299](docs/R1299-HANDOFF.md).
- **R1300:** guarded resident EFB surfaces, matching GPU scanout/raw restore and validated snapshot reuse. CPU overlap/unsupported states resolve to shared VRAM. [Details](docs/R1300-HANDOFF.md).
- **R1301:** Gekko2 naming and reachable direct relative PPC helper calls with far/unaligned fallback; all instruction-boundary checks retained. [Details](docs/R1301-HANDOFF.md).
- **R1302:** embedded launcher gecko wordmark and HBC icon. Guest runtime unchanged. [Details](docs/R1302-HANDOFF.md), [artwork](docs/BRANDING-R1302.md).

## Validation record

R1303: **200/200 native tests**, both cross-builds and four linked-PPC jobs. Additional tests cover all 35 early source-mutation exits for block lengths 2–8 and seven successful exits, including exact PC/retirement and all 18 nonvolatile PPC registers. Interpreter/JIT ALU/COP1 state and 66 full-VRAM signatures remain matched.

Under the same synthetic services, R1302→R1303 warm eight-instruction counts are ADDIU **3195→3177**, mixed COP1 **3181→3163**: 18 fewer PPC instructions (about 0.6%) in these workloads. This does not establish physical Wii FPS.


R1301 passed **200/200 native tests** and four linked-PPC jobs across Interpreter/JIT builds. The linked paths preserve **66 matching full-VRAM signatures**, ALU/COP1 state parity, and checks for budgets, interrupt boundaries, source mutation/TLB replacement and far callback fallback.

R1300 additionally checked resident framebuffer ownership and snapshot reuse with synthetic EFB/linked-PPC tests. These test services validate control/state behavior; they do not substitute for a physical GX GPU or establish Wii rendering speed.

R1302 passed both devkitPPC builds and native launcher visual inspection. The guest suite is inherited from R1301, **not rerun for the artwork-only revision**. See [public verification summary](docs/verification/R1303.json).

Warm eight-instruction R1301 benchmarks counted 45 fewer PPC instructions: ADDIU 3240→3195; mixed COP1 3226→3181. These isolated counts are not Wii cycle measurements or an overall FPS improvement.

## Performance and next work

No 10 FPS BIOS or 15–20 FPS Tekken target has been demonstrated. FPS counts emulated VBlank events per host second; OUTPUT counts host presentations, which can repeat the same scene. Neither counts unique images. Older logs and synthetic tests cannot predict current real-Wii FPS.

GX feature work is paused while the custom PPC dynarec becomes the priority. Next: profile-guided register residency/allocation, guarded memory/control blocks and corresponding IOP work, with precise interrupts, delay slots, exceptions and code invalidation. Keep the BIOS stable before expanding game work. Wii64/nullDC4Wii are reference projects, not integrated cores; Lightrec has not been adopted.

## Distribution and reporting

Public sources contain no BIOS, discs, private guest RAM/checkpoints or owner runtime logs. Preserve original source and third-party notices. Report revision, BIOS model, controller, JIT/interpreter and GX/software settings with timings/screenshots; redact personal paths and never share protected firmware/disc data.
# Current checkpoint: R1351C

DSI is now integrated into precise LW blocks and boundary classification.
Read docs/R1351C-DSI-BLOCKS.md. Actual Wii B log26 validates MMU self-test,
but had0 emitted sites; C fixes the execution-route integration. Physical
Wii C boot/backpatch timing is pending. Older A/B notes follow.

Paired official devkitPPC r50/GCC16.1.0/libogc3.1.0 builds. A is the guarded
memory control; B adds the MMU/DSI pilot for scalar aligned kernel-segment
EE LW reads. Both linked ELFs pass CPU, VU, FPU, scheduler and ARM transport
regressions. DSI HTAB/glue/backpatch checks pass with modeled privileged
hardware; no R1351 Wii speed/boot measurement exists yet. Read
docs/R1351-TOOLCHAIN-DSI.md for scope, settings, status codes and limitations.
ARM reply corruption and captured identical zero vertices remain open from
the physical R1350 log. Historical checkpoint notes below retain their dates.



### BETA12.13 GS alias / framebuffer coherence (diagnostic)
Native PSM-aware source bounds, conditional GX ownership resolve on physical texture/framebuffer overlap, 16-bit GS page geometry correction, and bounded 64KiB snapshot for overlapping `TRXDIR=2` cross-layout copies. Host/GIF regressions and Wii cross-build pass; real Wii behavior pending. See `docs/BETA12.13-GS-ALIAS.md`.

## BETA12.15 – targeted EE / IOP scheduler / GX fast-path update
- Cold-boot consistent EE cache-stride / way selection and retained owner guards.
- IOP one-tick native admission (AOT/JIT/scalar), callback unchanged.
- GX texturing invalidates the GPU texture cache only on an actual CPU
  texture-buffer upload; a rejected draw's pending upload is still handled.
- Host regression and Wii cross-build passed; physical hardware untested.
- Detailed scope and remaining work: docs/BETA12.15-JIT-SCHED-GX.md.

### BETA12.17 – JIT-Helfer, 128-Bit-GPR-Lokalität, GX-Framebuffer

- Bei EE-Exceptions oder TLB-Fehlern bricht die native Blockvorbereitung ab. Der DSI-LW/LWU-Helfer überschreibt das Zielregister nach einem fehlgeschlagenen C-Speicherzugriff nicht mehr.
- Die PPC-Registerresidenz gruppiert wiederholt gelesene vier 32-Bit-Lanes derselben 128-Bit-GPR innerhalb eines nativen Blocks. **Keine** dauerhafte Residenz über beliebige JIT-Blockgrenzen.
- GX-Import nutzt gesättigte Z16/Z24-Vergleiche; die späte FBA-/FBMSK-Verknüpfung unterstützt PSMCT32 und den Alpha-Erhalt bei PSMCT24. Komplexe GX-Fälle bleiben im bestehenden Software-Fallback.
- Cross-Build und gezielte Host-Tests bestanden; reale Wii, FPS, Crash/Tekken/GT3 bleiben zu verifizieren.

### BETA12.18 – GS scanout PSM und Exception-Vorbereitungen

- `DISPFB.PSM` 0/1/2/10 selects the Wii software scanout/probes; separate swizzled CT16/CT16S data and 5:5:5 RGB expansion. Existing CT32/CT24 scanout and GX presentation remain unchanged.
- `ee_core_block_prepare*` rejects a previous helper exception/TLB miss **before** peeking/fetching guest instructions; the exception vector is not overwritten by a second speculative fetch. Existing scalar fallbacks remain.
- Crash E3 disc `crash.bin` is a raw Mode2 2352-byte-sector ISO9660 image. Earlier Wii log requests `LEVELS/B/TITLES/TITLE.LGT;1`, but the disc contains only `TITLE.GSC;1` and `TITLE.NUS;1` in that folder. Resource absence may be intentional for an E3 demo; **not proof of the black screen's cause**. No fabricated title assets or forced framebuffer swapping.
- Host: `test_beta12_18_scanout`, `test_gs_display_scaling`, source guard-order checks pass; Wii cross-build, ELF/DOL structural verification and static MEM1 verification pass. No real Wii test or gameplay success yet.
- Open: actually reproducing/repairing Crash's CT32 black transition, full helper exception atomicity, persistent PPC registers across independent JIT blocks, complete GX DATE/blending/masks, exact framebuffer alias timing.


## BETA12.19 addendum (2026-10-15)
See `docs/BETA12.19-FB16-JIT-GUARD.md`: CT16/CT16S native framebuffer writes, FBA/FBMSK/DATE, depth saturation and EE JIT fault fences. Wii gameplay and independent-block 128-bit PPC pinning remain unverified/open.


### BETA12.20 – GX Alpha/DATE-Sprite-Import

Neue CPU-GX-Sprite-Verarbeitung für DATE, ATE/AFAIL, Alpha-Erhalt, FBA/FBMSK und spätes Z-Schreiben. 159 neue Testchecks + 324 GX-Pipelinefälle + 16 FB16-Checks bestanden. Wii-Cross-Build bestandene DOL-/ELF-/MEM1-Prüfung. **Keine** persistente 128-Bit-PPC-Registerübergabe über unabhängige JIT-Blöcke; siehe `docs/BETA12.20-GX-ALPHA-RESIDENCY-PLAN.md`. Crash E3 weiterhin unbestätigt.