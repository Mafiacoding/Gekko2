# BETA12.32: owner Wii evidence and BIOS reference review

Reviewed on 19 October 2026. Runtime artifact commit
`edf067455061a983d4257f95141caaf941f08954`; no new emulator binary in this review.
Owner logs, network captures, BIOS and reference images remain private.

## Observed Wii runs

Two owner BIOS sessions used GX and the same active CPU mask `00787dff`.
The receiver identifies the shipped runtime commit above.

| Observation | Session 1 | Session 2 |
| --- | ---: | ---: |
| Recorded PERF interval time | 405.252 s | 656.381 s |
| First-image event since session start | 258.201 s | 261.131 s |
| Chain / extended PSM / GX risk | OFF / OFF / OFF | ON / ON / ON |
| Completed ordered-token waits | 129,958 | 326,850 |
| GX timeouts / aborts | 0 / 0 | 0 / 0 |
| Native textured triangles accepted | 13,760 | 34,581 |
| Chain guest retirements | 0 | 41,922,425 |
| Extended PSM converted pixels | 0 | 0 |

Both logs end with an explicit controller-requested return to HBC, not a
reported GX fault. This is real owner Wii evidence of successful completion
barriers and sustained rendering in these two runs, including combined opt-ins.
It does not establish universal GPU stability, pixel correctness or game support.
The former missing-token failure was not reproduced in these sessions; its
exclusive cause and coverage outside these BIOS runs are not proven.

The second session's risk route records 34,133 fast draws and 448 fallbacks.
The chain route is actually exercised, rather than merely compiled or selected.
The PSM preference is active but no local format conversion occurs, so this
capture cannot validate those conversions or show a benefit from enabling them.
Defaults remain OFF. The two sessions change three options together and have
different durations/scenes: neither option's individual speed effect can be
measured from this comparison. First-image events also use a detection probe,
not a fixed-frame visual comparison.

The network receiver recorded 118 valid packets, 118 ACK sends, no duplicates,
no invalid packets and no receiver/ACK errors. Runtime transport error/drop
counters remain zero. There is one missing sequence number; captured interval
time is incomplete. Capture covers session 2 through roughly 602 seconds;
the SD log covers its later tail. An ACK sent by Windows is not independently
proof of every ACK being received by the Wii.

## Speed conclusions

In session 2, cumulative GX_SYNC time is approximately 2.127 seconds:
submission 1.289 s, presentation fence 0.718 s, readback fence 0.119 s.
These are recorded wait scopes, not all GX work, GPU utilization or command
submission overhead. Nevertheless, spending most of the session blocked on
these completion fences is inconsistent with the counters. Do not remove the
working guards to address the large remaining speed gap.

Existing `tools/analyze_runtime_profile.py` was run separately on both complete
SD logs. In session 2's sampled CPU scope, GS raster accounts for 23.85%, EE
dispatch/scalar for 23.56%, scheduler for 20.02%, native attempts for 18.02%,
and IOP for 10.99%. These shares belong to the sampled CPU denominator; they
are not whole-run utilization and do not mix with exact presentation samples.
The partial network window yields different shares and must not substitute
for the full SD log. No new measured Wii FPS increase follows from these runs.

## BIOS image investigation

Follow-up: [BETA12.33 local reference investigation](BETA12.33-GX-DEADLINES.md)
now includes actual independent PCSX2 software replay, which also lacks the
bright small squares for the privately captured command stream. The earlier
host reproduction below is not independent proof of a GS/geometry defect.


The supplied PCSX2 reference visibly contains bright small square faces around
the blue animation and shaded larger cubes. Those small faces are absent or
much weaker in the owner Wii photograph. Different phases, capture methods
and unknown PCSX2 settings preclude a numerical pixel/brightness comparison.
Do not apply a global brightness boost, fabricate squares or change display
aspect ratio as a substitute for identifying the GS/geometry error.

A fresh local cold boot using the current source and the owner's unchanged
BIOS reproduced dark/incomplete-looking surfaces in software output, including
a captured phase with the Sony text. Command template:

```
python3 tools/investigate_bios.py --bios PRIVATE_BIOS --output PRIVATE_OUTSIDE_REPO --slices 65
```

Actual result: exit 0, 65 slices, 10 captured frames, neither EE nor IOP halted;
no guest patch or injected picture. All output stays outside the checkout.
This is host execution and software GS, not an execution of the Wii PPC JIT or
physical GX. The diagnostic driver also enables its existing host event/idle
options; it is not an identical Wii configuration or timing oracle. This
supports investigating a common rendering/geometry cause without another long
owner run, but does not independently isolate the missing-face cause.

Next isolation work should follow actual guest draws: geometry/coverage and
depth rejection, sampled texture RGB/alpha and TFX, then framebuffer blending
and scanout. Use the private local reproduction for that work and derive
synthetic public regressions only after a specific discrepancy is found.
Sampled DATE rejection counters remain zero in both Wii logs; the final
transport snapshots show no pending XGKICK or unknown VU pair. Neither finding
proves every earlier draw/transport state correct. No new tile fix is claimed.

## Validation and delivery boundary

This review changes documentation only. The host cold boot and two profile
analyses above were executed now; no new host regression suite, Wii build,
Dolphin run or physical test by the agent is claimed. The owner provided the
physical Wii evidence. Keep the existing BETA12.32 download and rollback build;
do not relabel an unchanged DOL as a new graphics fix.
