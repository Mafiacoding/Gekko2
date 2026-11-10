# BETA12.24 – bounded game transport diagnostics

16 October 2026. Branch `codex/beta12-24-game-diagnostics`, based on
`bdd8230ba892de24f1a43c0218a9125a387e5ab3` (BETA12.23).
Release `R1389-SPEED-BETA12.24-GAME-TRACE`.

This is a diagnostic build for the owner's Crash E3/Tekken progression
failures. It does not contain a new gameplay or SIGNAL-stall bypass.
BETA12.23 graphics, VU/VIF fixes and aspect correction are retained.

## What the owner should do

Install the separate BETA12.24 app, retaining BETA12.23 for rollback. Cold Boot
Crash with both experiments OFF, reproduce the failure and leave it running
for another 15–30 seconds, then pause normally. Repeat for Tekken. Return the
new numbered `BETA12.24-GAME-TRACE-...-disc-gx.log` files privately. Note the
visible screen/stage and controller input. Do not spend hours in an unchanged
stall: the trace summarizes transitions and snapshots, not every retry.

The normal session log location remains `sd:/pcsx2/logs/`. No separate logging
configuration is required for this DOL. Both experimental options retain their
saved Cold Boot behavior; the diagnostic mode does not switch either ON.
No BIOS/disc, full RAM dump or AOT profile needs to be supplied.

## Recorded observations

- `GAME_CONTEXT`: sanitized selected image basename, not an asserted title or
  game-version fingerprint; directories are omitted. Existing numbered session
  boundaries and cold boots distinguish runs.
- `GAME_EVENT`: 64-entry rolling RAM history of accepted GIF tags (separate
  PATH1/2/3 labels with all four tag words), XGKICK starts, SIGNAL writes,
  SIGNAL wait/resume transitions, GS CSR acknowledgements/IMR changes,
  GS INTC acknowledgements/mask changes and VIF wait/resume conditions.
- GIF refusal reasons: 1 pause, 2 another path owns GIF, 3 PATH3 mask,
  4 pending higher-priority path vs PATH3, 5 PATH1 priority vs PATH2.
  Repeated identical refusals do not each generate an event.
- SIGNAL events retain path, ID and mask; SIGNAL_WAIT retains path, remaining
  payload count and the unaccepted ID/mask. This distinguishes a real second
  SIGNAL from bad packet/register interpretation. Acknowledging INTC_GS and
  acknowledging GS CSR.SIGNAL are deliberately separate event types.
- `GAME_UNKNOWN`: first eight distinct VU1 upper/lower pairs which failed
  decoding, including VU PC and decoded-half bits (2 upper, 1 lower).
  These are pinned separately so subsequent traffic cannot overwrite them
  before the next log snapshot. They can also appear in GAME_EVENT with the
  same sequence; deduplicate by sequence. An unknown encoding can result from
  invalid/corrupt program data, not necessarily a missing legal instruction.
- `GAME_CPU`: EE PC/next/EPC/Status/Cause and selected low GPR words (SP, RA,
  V0/V1/A0/A1) at the snapshot. `GAME_WAIT` records IOP/VU PCs, path owner,
  interval VBlank count, cumulative GIF qwords and SIGNAL retries. Together
  with existing DMA/GIF/VIF/GS state, these locate a persistent wait.

All producers run on the existing single emulation thread, use fixed storage,
and do not allocate, write SD, fetch guest memory, invoke MMIO or change guest
CPU/hardware state. Timestamps use EE retirement, not Wii wall time and not
independent VU cycles. No per-instruction trace or unbounded queue is added.

The next existing performance or progress/pause/exit checkpoint drains new
entries to the session log, at most 64 rolling entries plus eight newly pinned
unknown pairs. `GAME_TRACE overwritten=...` explicitly reports missing history.
Repeated snapshots without events do not duplicate the ring. Cold Boot resets
sequence/history/cursors. This is bounded recent history, not a lossless full
execution recording or a guarantee of SD persistence after power loss.

## Reading a stall

A useful sequence is TAG_PATH1 → SIGNAL → SIGNAL_WAIT → CSR_ACK →
SIGNAL_RESUME. Missing CSR_ACK plus a repeated EE wait PC suggests looking at
interrupt/handler delivery; an implausible tag and mismatched register position
suggests checking VU/GIF producer data. Neither observation alone proves the
root cause. VIF_WAIT then VIF_RESUME verifies a real microprogram interlock.
Never clear a pending SIGNAL, invent EOP or skip a game loop just to advance.

`python3 tools/analyze_game_trace.py <private-log>` summarizes event counts,
loss and unmatched SIGNAL waits locally. `--self-test` validates parsing.
An unmatched wait at the end can simply be a paused in-flight transfer; the
parser explicitly does not label it a deadlock or claim in-game detection.
User filenames, tag/instruction words and registers remain private diagnostics;
do not commit user logs or attach them to public PRs/downloads.

## Build and validation

`tools/build_beta12_24_trace.sh` uses a separate build directory and retains
BETA12.23 SPEED/DSI/AOT/GX/ARM/idle/Lightrec and chain-capability flags, adding
`-DGEKKO2_GAME_TRACE`. Compile without that flag for inline no-op trace calls.
This delivered build includes tracing; BETA12.23 is the rollback/comparison DOL.
There is no measured claim of zero overhead or higher FPS from logging.

Pinned SDK: devkitPPC r50/GCC16.1.0/libogc3.1.0, official image
`devkitpro/devkitppc@sha256:4c919aa26151dd43d88ca28c922d1fe2409579a8ba60ef56517baf1abdfb1a48`.
DOL/ELF section checks pass; static MEM1 remaining is 2,694,988 bytes.
DOL SHA256: `894efdfea9114590b829ae942d2c13fcdcb2c077acf2c6f79da6ee91a3e91bac`.

Validation includes all 47 BETA12.23 host suites with GAME_TRACE compiled ON,
a ring test for empty/overflow/order/no-repeat/reset/CPU state preservation
and pinned unknown retention, a real GIF transport test proving 1,000 stalled
SIGNAL retries produce one wait event and resume only after real CSR W1C,
and parser self-tests. Linked PPC VIF/UV/aspect, XGKICK/palette/IPU/controller,
VIF DIRECT/GS and GX fence checks run against the final ELF; exact outputs
accompany the artifacts. GPU/platform services are modeled in those fixtures.

No physical Wii BETA12.24 run or confirmed Crash/Tekken gameplay is claimed.
This build supplies the missing event sequence needed for the next reproducible
fix; it is not proof that the existing SIGNAL/XGKICK stall is resolved.
