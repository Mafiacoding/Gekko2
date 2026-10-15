# Gekko2 R1369 Beta: EE event-window chains

Base: the exact saved Gekko2-R1368-Source.zip. The source and current compiler were restored after the transient working copy expired. Both comparison builds use devkitPPC GCC 16.1.0 and libogc 3.1.0 from the same verified SDK layer.

## Implementation

The existing native-event window path can now call already-compiled successor windows without returning through the outer EE dispatcher after each window. The emitted PPC chain maintains state, total retired instructions, remaining grant, callee and expected retirement count in preserved registers. A declined or partially admitted window ends the chain. The resolver never compiles or allocates and only accepts complete eligible successor owners. Mixed/helper/store/branch owners remain precise boundaries.

Each successor still validates owner serial/function/PC, source and mapping epochs, a live first instruction, current EE state and the available interleave budget. Its native-window admission retains timer, interrupt, DMA/SIF, display and special-PC guards. IOP catch-up happens through the existing interleave accounting before a successor is admitted. Active chains pin owners. Cache reset releases the new chain thunk.

Fully admitted, non-memory arithmetic windows additionally have one straight-line body with one register-allocation region, rather than a budget comparison and independent allocation after every guest instruction. The existing partial-budget body remains available. Dirty architectural words are canonical at the end of each window; helpers and separate windows still observe RAM state. This does not keep guest registers resident across arbitrary block/helper calls or combine branch traces.

The existing Native Links option controls chain admission and compilation of the full-window body; Native Events and EE Services are required. Use a cold boot after changing these settings so cached bodies are rebuilt. Other option defaults and clocks are unchanged. ARM, VU and IOP service semantics are unchanged.

Log line: `EE_EVENT_CHAIN enabled=... runs=... retired=... selected_successors=...`. The last field counts selected cached continuations, including a continuation that its subsequent event-admission guard declines. Retired counts count actual guest work, not selected owners. The enabled field reports the option route, not hardware confirmation.

## Validation

39 native regression tests passed, preserving R1368 graphics, checkpoint, input, audio and memory tests. 120 linked-PPC comparisons check complete EE/timer/display/IRQ state against the precise path across partial/full budgets, changed first instructions, notified interior code writes and Compare interrupts. Fourteen additional comparisons cover evolving proven RAM loads and callee-saved ABI. Twenty-six full EE/IOP/timer comparisons and six shared-mailbox cases pass. Existing mixed-prefix, arena-pressure, native-chain ABI, scheduler/IOP mailbox and GX publication regressions are also retained. Physical Wii FPS and boot-time gains remain untested.

The successor thunk and learned edges are warmed before this measurement; their first-time compilation is excluded.

Warm complete arithmetic windows, 100 repetitions after warming, using the same recovered compiler for both releases:

| Guest instructions per window | R1368 PPC instructions | R1369 PPC instructions | Reduction |
| --- | ---: | ---: | ---: |
| 8 | 102542 | 102042 | 0.49% |
| 16 | 108942 | 105842 | 2.85% |
| 32 | 121642 | 113342 | 6.82% |

A warm sequence of 16 cached windows (128 guest instructions total) takes 21,690 PPC instructions in R1368 and 13,516 in R1369: 37.69% less. The complete EE/timer/display/IRQ state SHA-256 matches: `1c753cba87cb5cebdc4b6d72ddcea9b82a7e651ddb2c1a4bc26e7c7924211291`. Fifteen successor windows are selected. Callee-saved register and CR preservation is checked on the actual chain. These gains overlap with the full-window optimization and must not be added together.

The complete EE-state hashes and guest retirement totals match in each case. Counts cover this synthetic CPU path, not whole-emulator time or physical Wii FPS. Short/mixed windows can pay extra chain/branch overhead; additional native bodies consume cache space and compilation work. A/B testing remains necessary.

## Wii test

Cold boot with the same BIOS/game and settings as R1368. Keep Native Events, EE Services and Native Links enabled for this route. Record the first-image elapsed time and stable EE_per_s/guest_vblank values at equivalent guest progress, and retain the full log. For an option control run, disable Native Links and cold boot again. Do not infer a speedup from the EE instruction count alone.
