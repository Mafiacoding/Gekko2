# R1287 — ELF memory safety checkpoint

R1287 includes all restored R1285 work, the R1286 GS bounds fix, and additional IOP ELF/DMA bounds corrections.

## Changes

- GS color/depth guards reject wrapped offset+width values; R1285's old guard could read/write globals immediately before VRAM.
- IOP ELF program/section tables and file-backed segment/relocation ranges are validated before offset arithmetic. Filesz cannot exceed memsz; relocation targets and paired LO16 targets must fit IOP RAM. Module names stay inside their section.
- DMA main RAM and scratchpad ranges reject overflowing lengths and undersized bindings.

## Validation

All 185 native regression tests pass, including 8 new DMA range cases and 10 new malformed IOP loader cases. Results and individual logs are included in the checkpoint. Actual linked PowerPC ELF tests execute 1 valid and 9 malformed IOP module cases per build, including wrapped relocation targets; unchanged RAM is checked on rejection. GS bounds tests execute 42 cases per build plus the inherited color/depth checks. Complete software draw paths and CPU/JIT profile integration are also exercised. Sanitizer and real-PPC checks from R1286 cover the inherited GS fix.

This does not establish absence of every emulator bug. No R1287 real-Wii cold boot, stable OSDSYS menu, or FPS improvement is claimed. The full JIT and a GX GS primitive renderer remain incomplete; GX is optional presentation only. GitHub publication is pending and was deferred to concentrate on the ELF.

## Install

Use PCSX2-Wii-R1287-Menu-JIT.dol as sd:/apps/pcsx2-wii/boot.dol. The ELF is supplied for Dolphin/debugging. Interpreter builds are included for comparison. Preserve your BIOS/config/discs; no BIOS or game image is included. Boot logging uses sd:/pcsx2/R1287-boot.log.

Claude.patch and Claude.diff apply to the original uploaded pcsx2-wii-full-debug(1).zip source. They are byte-verified against the current selected source files. The checkpoint contains the binaries, cumulative source patches, handoff, licenses and verification results.
