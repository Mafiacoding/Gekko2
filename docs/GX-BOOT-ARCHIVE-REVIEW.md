# Owner archive review — 19 October 2026

Ten log files were privately extracted with bounded sizes and flat, log-only
paths. None of the raw files, archive or guest data are committed. The owner
confirms `crash.bin` is the Crash E3 demo. No Tekken-labelled session appears.
The BETA12.33 file records the new hidden-HUD loading flow but contains no
source commit identifier; the exact runtime commit cannot be inferred from
the reused release name alone.

## Findings

- Four disc sessions identify Crash by selected filename: three BETA12.32,
  one BETA12.33. One additional BETA12.32 disc session selects GT2000.
- All these disc sessions end without a first-image event; display circuits
  remain disabled (PMODE zero). The current Crash session has zero submitted
  guest copies, GX draws and GX waits/timeouts. This places its observed stop
  before native GX rendering. It does not prove that all GX configurations or
  all previous game builds are unaffected.
- The three older Crash runs already show this pre-picture state; the new
  loading overlay is not the earliest recorded occurrence in this archive.
- Current Crash EE retirement reaches about 427.4 million, IOP about 53.4
  million ticks, without either CPU's halted flag. Persistent sampling of a
  small code region is not enough to identify the blocking instruction or
  missing service. GIF receives 97 Qwords and VU1 does not begin execution.
- The current trace records one failed optional-looking error-file OPEN
  and no font OPEN or READ. Missing error-file status is not established as
  causal; do not synthesize files, force a positive reply or skip guest code.
- VBlank is **not absent**: summing interval counters gives 86 events.
  `GAME_WAIT.vblank_events` has always received the interval delta, not a
  lifetime total. A zero in one five-second interval is expected at these
  very low emulation rates. Do not force interrupts based on that zero.
- In the current Crash log, accumulated PERF interval time is 885.269 s,
  core scope 787.678 s, presentation scope 79.546 s. These scopes leave
  logging/other overhead and are not a controlled cross-build speed result.
  Repainting the loading screen is material overhead, but removing it alone
  cannot establish the solution to this boot stall.
- A separate BETA12.32 BIOS session has a real completion-fence failure:
  655,467 waits, 655,466 finish callbacks, one timeout and one abort. It pauses
  with an established image instead of safely resuming discarded GPU work.
  Other BIOS sessions complete without a timeout. The archive lacks the
  companion timeout register/token diagnostic, so neither lost interrupt,
  malformed FIFO nor hardware stall can be isolated from these counts alone.

## Changes supported by the evidence

The preceding [loading refresh fix](LOADING-REFRESH-REGRESSION.md) removes
recurring full-frame erase/rebuild; Wii visual acceptance remains open.

Each new SD session now includes its compiled source commit and whether the
chain-residency variant was compiled, independently of the mutable release
label. The existing VBlank delta is retained for compatibility, explicitly
labelled as an interval, with a new lifetime total. This adds information to
the existing session file instead of requiring a separate startup capture.
No guest CPU, GS timing, interrupts or GX completion guards are changed.

A new actual linked-PPC test crosses the free-running frame deadline with
PMODE off at ten admission distances for interpreter, owner-mask JIT and
native-window masks: **30 cases PASS** on runtime `852600c`. This independently
checks a plausible early-boot dependency without asserting that the owner's
exact game loop has been reproduced. Session-file tests cover default/explicit
build IDs, both compile variants, exclusive naming, resume and write failures.

## Blocked reproduction

The local private assets currently include the owner BIOS and GT2000 demo,
not the actual Crash E3 image. A private direct link to the exact image has
been requested for local reproduction and before/after tests. No new Wii
log run is requested. GT2000 is not a substitute for reproducing Crash.
Tekken's newly reported regression remains unverified by this archive.

Crash, Tekken and the BIOS GX timeout remain OPEN. This investigation does not
publish an alpha or establish a game-start, pixel-correctness or FPS fix.
