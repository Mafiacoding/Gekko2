# Alpha frontend and GitHub cleanup — 19 October 2026

Runtime source: `33b057f`, branch `codex/alpha-frontend-github-cleanup`.
Consolidated [draft #51](https://github.com/Mafiacoding/Gekko2/pull/51) targets main.

## Behavior

- Replace forced diagnostic HUD before first guest picture with a native XFB
  loading screen: original logo, actual boot stage, elapsed time and activity
  indicator. No fake completion percentage or guest graphics are synthesized.
- Existing saved HUD setting is now named **On-screen logs**. Default OFF;
  old ON preferences remain ON. The menu persists it on SD, and controller
  shortcuts temporarily change visibility. No new redundant menu switch.
- Gate SDK stdout/stderr console devices, leaving file and network devices
  untouched. Suppressed output is discarded, not buffered for later replay.
  The launcher itself uses direct text rendering independently of the gate.
- Storage, BIOS, initialization, disc, halted/stopped and memory-card failures
  have explicit user-facing screens; failed card flushing still blocks exit.
- Keep SD startup/session logs and the separate default-OFF network logger.
  This switch controls display, not whether diagnostic files exist.

## Public repository

Documentation-only [PR #52](https://github.com/Mafiacoding/Gekko2/pull/52)
was merged into main as `999df60`. It changes only README and its historical
archive. Main remains runtime BETA12.20 and explicitly links the newer candidate.
No emulator code was merged and no public alpha was released.

The candidate README has one version-independent entry point, a documentation
index, installation/settings guide and build guide. Historical notes, gallery
and credits remain archived. Both entry points explicitly require **current
official devkitPro/devkitPPC and libogc**, distinguishing recorded tested
r50/GCC16.1.0/libogc3.1.0 from future SDK updates.

31 older Codex drafts were verified by exact head ancestry and closed as
superseded by #51; see [preserved PR/branch index](PR-INDEX.md). Other open PRs
were not proven ancestors and remain open. No branch, download or release was
deleted. Updating the separate GitHub About description was denied HTTP 403
by the available integration; README publication succeeded.

## Validation

90 host regression groups PASS at `a4114ba`. The final runtime differs only
in the loading title's font scale (the renderer supports 1 and 3, not 2);
there is no subsequent host-tested core/header change. Final ELF-specific
checks below validate the frontend correction as well.

Both fresh control and chain Wii builds use devkitPPC r50 / GCC16.1.0 /
libogc3.1.0 and retain previous SPEED/AOT/DSI/GX-fence flags. DOL/ELF identity
and static MEM1 gates pass: control 2,336,268 bytes remaining; chain 2,199,564.
These are static headroom figures, not peak runtime allocation measurements.

192 console routing cases execute actual linked PPC gates (96 per ELF),
covering disabled discard, enabled delegation, partial writes and errors.
SDK writer callbacks are synthetic; no physical Wii execution is implied.
Four linked loading/error renders at 640×480 and 640×576 pass: visible title
glyphs and bounded XFB writes. Pixel drawing executes the actual linked PPC
function; video/cache services and elapsed-label formatting are stubbed.
All six linked EE/scheduler groups also PASS on the final ELFs: GetError RPC,
masked native-window differential checks and event/mailbox scheduler checks
for both control and chain builds. Together with the frontend gates this is
nine linked-PPC groups. [Machine-readable evidence](verification/ALPHA-FRONTEND-RESULTS.json).

## Acceptance still open

No new real Wii or Dolphin run. Verify the loading/error screens and saved
ON/OFF behavior on hardware before public alpha acceptance. No new measured
FPS gain, fixed BIOS tiles or playable games are claimed. Existing alpha
blockers remain in [the readiness report](ALPHA-READINESS-2026-10-19.md).
No owner BIOS, discs, captures, checkpoints or SDK binaries are in these changes.

A fresh private SD review package is generated only after the host, all six core
linked groups, two console groups, loading renders and both static gates pass.
The packager verifies each ELF hash and embeds the user guide and frontend
report. It does not publish a release or overwrite saved SD options.

Packaging negative check PASS: an intentionally mismatched console/ELF hash
is rejected before ZIP creation. Flattened packaged guide links are pinned to
the recorded GitHub source commit, rather than broken relative SD ZIP paths.
