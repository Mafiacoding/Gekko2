# R1355: masked CPU interrupt deadlines

R1354 baseline, devkitPPC r50 / GCC 16.1.0, libogc 3.1.0.

## Change

An asserted EE INTC/DMAC hardware level previously forced every event-scheduler window to eight EE slots even when CP0 Status prevented delivering that interrupt. R1355 applies the same IE/EIE, EXL/ERL and IM2/IM3 predicates as the project's precise interrupt handlers. Masked levels may use the existing maximum of 64 slots. The 8:1 EE/IOP slot ratio, timer deadlines, Count interrupts, SIF/RPC barriers and urgent notifications remain intact. This applies only with Event Scheduler enabled. Native window admission remains conservative: pending lines still decline the accelerated native retirement path. No pending Cause bits or latch delays are suppressed.

EE_DEADLINES reports deadline query counts: rpc, idle, deliverable_irq, masked_irq, count_irq, timer_near, short and wide. masked_irq overlaps later classifications; these counters are not time percentages. Reset on cold boot.

## Existing-log evidence

Logs 32/33 are R1354: first output ~295.4M EE / 371.5-371.9s; neither reaches 1B. New IOP batching reduced lookups but did not materially improve boot time.

Older R1345 log 19 reaches 1.74B and already reports FIRST_IMAGE=1 at 900M, 1B and 1.1B. At 900M one sampled interval reports 986785 EE/s; around 1B one interval reports 57604 EE/s. EE translation installs increase from 6959162 to 11481365; collision count from 6960390 to 10713978. Pressure sweeps remain zero: do not label this as proven code-arena exhaustion. Changing workloads/idle behavior also affect EE/s, and near-constant 0.198Hz guest VBlank means instruction throughput alone does not quantify guest speed. This is an old-build observation, not proof of the same failure in R1354.

## Reference

PCSX2 pcsx2/R5900.cpp, cpuIntsEnabled: checks the selected interrupt mask, EIE/IE and EXL/ERL before delivery. https://github.com/PCSX2/pcsx2/blob/master/pcsx2/R5900.cpp (retrieved 2026-10-09). This is a correctness predicate reference, not a port of PCSX2 cycle scheduling.

Prior Play HLE, Android, nullDC4Wii cache/DSI references are preserved in R1354-REFERENCE-REVIEW.md. No Android CPU backend or ARM EE/IOP execution is added. ARM worker support and DSI remain inherited, and existing VU/GIF zero geometry is not claimed fixed.

## Wii test

Use the same R1354 settings, with Event Scheduler on. Cold Boot; keep options constant. Capture first BIOS output, then continue beyond 1.1B EE if practicable. Compare actual boot milliseconds, guest_vblank_mHz, EE_DEADLINES, BLOCK_CACHE deltas and EE_CACHE_PRESSURE. A speed increase has not been measured on real Wii.
