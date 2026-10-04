# R1280 — real-Wii BIOS alpha controls, FPS, diagnostics and EE cache

User hardware confirmation: R1279 reached the real Sony Computer Entertainment
screen on the Wii. This confirms the splash, not yet a stable OSDSYS menu or
a hardware FPS target. BIOS stability/performance stays ahead of games.

## Wii Remote idle bug fixed
R1279 called WPAD_SetIdleTimeout(0). Disassembly and execution of the supplied
libogc 1.8.18 timer show it compares incremented idle >= timeout with no zero
special case: zero disconnects the Remote on the next idle alarm. R1280 uses
UINT32_MAX (about 136 years), effectively no idle shutdown in an emulator session.
The real linked timer was tested with a synthetic connected Remote: zero
calls disconnect; UINT32_MAX does not over 601 ticks. Bluetooth calls are mocked;
physical pairing/reconnection still needs Wii testing. No batteries or pairing
records are modified. Remote alone, optional Nunchuk and GameCube pad remain.

## FPS / HBC controls
- SETTINGS: **2** (GameCube Y) toggles the separate FPS counter, initially ON.
  A toggles the diagnostic HUD; 1 / GameCube X toggles scheduling target.
- Counter **FPS** = emulated VBlank events / host wall time. **OUTPUT** = host
  presentations / wall time, which can include repeated images. Neither is
  counted as unique rendered scene content. The rates use five-second samples;
  very slow boot can show 0.00 between sparse VBlank events. Values are not a
  promise of performance or real PS2 timing accuracy. Settings are session-only.
- HOME during emulation pauses into the emulator launcher. HOME + MINUS exits
  to HBC from boot, browser, settings, prompts and launcher.
- HOME in the launcher main page, or **EXIT TO HBC** + A, exits. HOME in another
  launcher page returns to its main page. GameCube B+Z+START also exits.
- Exit writes final progress, releases selected disc, cores/browser/BIOS and
  calls standard exit(0), using the return stub supplied when launched by HBC.
  This has been built, but actual return into HBC must be checked on the Wii.

## Host scheduling/presentation
The previous fixed 50,000 IOP-slice redraw budget could block input for seconds
in a slow BIOS phase. R1280 starts with 512 slices and adapts toward 50 ms core
chunks (20 ms in responsive mode), bounded 32..50,000, with at most 2x growth.
Guest instruction order and eight EE instructions / one IOP instruction remain
unchanged. An individual costly guest instruction/VU invocation can exceed the
host target; this is not a hard latency guarantee or a guest-clock speed hack.
Input is polled per chunk. Wii VSync waiting is moved to presentations.

Frequent input polls do not require a full GS-to-XFB blit. Guest VBlank triggers
presentation; GIF quadword activity or display-register changes refresh at the
500 ms check. An unchanged framebuffer has a five-second fallback for writes
outside normal GIF paths. HUD/progress refresh at 500 ms. Frame and FPS overlay
use a single full-XFB flush. No artificial high-resolution GS configuration or
synthetic BIOS pixels are introduced.

## EE JIT optimization
The warm L0 path previously checked PC+instruction twice (first negative entry,
then positive). It now validates both once before choosing positive/negative.
All compiled-code ownership, counters, instruction-word validation and fallback
semantics remain. Actual PPC full-retirement memory tests (eight instructions):

| Operation | R1279 warm PPC instructions | R1280 | Reduction |
|---|---:|---:|---:|
| LB | 4753 | 4729 | 0.50% |
| LH | 4777 | 4753 | 0.50% |
| LW | 4769 | 4745 | 0.50% |
| LWU | 4761 | 4737 | 0.50% |
| SW | 4737 | 4713 | 0.51% |
| SD | 5297 | 5273 | 0.45% |

This is 3 PPC instructions saved per warm cache dispatch, not measured real-Wii
cycles/FPS, not a whole-BIOS workload benchmark, and not a completed JIT port.
EE and IOP still predominantly use single-instruction native dispatch; efficient
multi-instruction blocks/register allocation and remaining VU pipeline/flag/Q/P/
EFU coverage require further work. Choose the next changes from real BIOS logs.

## SD diagnostics / installation
Replace sd:/apps/pcsx2-wii/boot.dol with the R1280 JIT DOL renamed boot.dol.
Keep the BIOS, games and bios-config.bin in place. Debug output is
**sd:/pcsx2/R1280-boot.log**: five-second PERF samples, guest/host rates,
EE instructions/sec, core/blit times, adaptive budget, Remote status, EE/IOP PCs,
EE JIT executed/compiled/rejected and L0 hits/misses, IOP and VU counts.
Normal progress and first-image/fault/exit evidence are retained. A new cold
boot truncates the version log; resuming appends. R1252-first-fault.txt is still
written once if a real recorded fault is found. Upload the boot log after testing
especially across Sony/SCEA/OSDSYS, to localize core versus rendering slowdown.

## Validation
- Both Interpreter and JIT ELF/DOL builds with -j1.
- 183 native regression tests pass, including rates/budget/cadence tests.
- Actual final Wii ELF Remote/Nunchuk, old-libogc idle-timer, five FPS cases,
  64-bit presentation cases and 1000 randomized adaptive budgets, both builds.
- Inherited actual PPC EE/IOP/VU/JIT/GS/font/memory tests, both final ELF builds,
  including self-modifying cache words, negative-cache and allocation retry.
- Native warm BIOS checkpoint advanced from EE 2,051,427,132 to 2,054,623,152
  without an EE/IOP halt. This is a resumed native interpreter test, not fresh
  cold boot on the Wii or PPC BIOS execution. Private checkpoint stored separately.
- Launcher/settings previews rendered from current C layout and inspected.
  These previews are host renders, not hardware screenshots.
- Cumulative Claude patch dry-run/apply and changed-file byte comparison;
  public checkpoint ZIP CRC and SHA256 manifest verified. No BIOS/game/raw
  guest memory in the public archive.
