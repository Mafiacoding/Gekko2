# R1265 OSDSYS configuration persistence

The pre-R1265 controller path supplied placeholder acknowledgments and zero
configuration data. R1265 replaces this family with a shared bank model,
validated disk persistence and actual provider block counts. See R1265-HANDOFF.md.

## Verified protocol

Primary SDK client: `ps2dev/ps2sdk/ee/rpc/cdvd/src/scmd.c`.
- RPC 0x0e: OpenConfig packs mode | bank<<8 | count<<16 into four bytes.
- RPC 0x0f: CloseConfig, reply two words.
- RPC 0x10: ReadConfig, reply 0x408 bytes; data starts at +8, 15 bytes/block.
- RPC 0x11: WriteConfig, request 0x400 bytes containing 15 bytes/block.
- Reply word 0 is the provider return value; word 1 is status. Reading or
  writing multiple blocks must return the actual completed block count.

The uploaded SCPH-50004 ROM's XCDVDMAN was inspected locally, without
changing or redistributing firmware. Offsets below are relative to its
ELF text segment (not loaded IOP addresses):
- OpenConfig 0x8118 sends mode, bank, count through SCMD 0x40; the
  requested block count is stored for the subsequent multi-block calls.
- CloseConfig 0x81ac sends SCMD 0x43 and clears the stored count.
- Single-block read 0x81e8 sends SCMD 0x41, reads 16 bytes, computes
  `sum(data[0:15]) & 255`, compares with byte 15, reports checksum status,
  and copies only 15 bytes to the caller.
- Multi-block read 0x8300 calls the single-block function, advances the
  output by 15 bytes, and returns completed blocks on error or completion.
- Single-block write 0x83e0 copies 15 bytes, appends their modulo-256 sum
  at byte 15, then sends the resulting 16-byte block through SCMD 0x42.
- Multi-block write 0x84d8 advances input by 15 bytes and returns completed
  blocks. This resolves the earlier checksum/provider uncertainty.

Bundled PCSX2 primary reference `CDVD/CDVD.cpp`, cdvdReadConfig /
cdvdWriteConfig and SCMD cases 0x40..0x43:
- Three banks with 4, 2 and 7 hardware blocks respectively, 16 bytes/block.
- Session tracks read/write mode, bank, requested count and current block.
- Close clears the session. Wrong mode and exhausted requested count fail.
- Requested blocks outside a bank's implemented range are handled explicitly.
- Do not copy PCSX2's fastboot initialized-flag workaround. Initialization
  should come from actual OSDSYS settings writes.

## Implemented in R1265

Implemented one shared configuration-bank/session model for actual IOP MMIO
and EE HLE RPC. Strip/check or generate checksum only at the RPC/provider
boundary; hardware storage remains 16 bytes/block. Bound every request
and reply to the real transfer sizes. Persist accepted writes on SD with
validated load and safe replacement, retain storage across soft IOP resets,
and serialize session/data separately from host file paths in checkpoints.
Regression fixtures cover multiple banks/blocks, checksums, wrong modes, limits,
malformed files, backup recovery, RPC sizes and checkpoints. Controller-only
OSDSYS editing followed by a genuinely fresh BIOS boot is recorded in the handoff.
A placeholder success reply or forced initialized flag is not persistence.
