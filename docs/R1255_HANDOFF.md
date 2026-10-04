# R1255 checkpoint — 2026-10-02

## Status / user goal
Diskless OSDSYS menu is NOT fixed or visually verified. Do not claim success or
5 FPS. Do not enable the legacy browser RAM injection. JIT optimization is
still deferred until the actual menu works. The JIT binary uses the existing
JIT, with these correctness changes only.

## Verified new changes
- RPC descriptor lifecycle: validate client/pkt/rpc_id before publishing BIND
  state; clear allocation bit and packet rpc_id to permit packet-pool reuse.
  Retain cd.pkt_addr until the real guest REND handler consumes it: nulling it
  early caused a real BIOS store fault at address 0x10 / EPC 0x000843e0.
  Correct client buf/cbuf offsets are +0x14/+0x18, not reply +0x28/+0x2c.
  This remains a companion to the existing HLE semaphore bridge and real DMA
  reply; it is not a complete SIF-RPC/async end-function implementation.
- FRAME.FBMSK, per-context ownership/activation, RGB24 preserves stored alpha
  while blending uses fixed destination alpha 128. FRAME 16-bit output and
  DATE are still incomplete.
- GS IMR uses actual mask bits 8..12, VSMSK bit11. CSR interrupt status bit3
  stays VSINT. Reset masks the five sources. Reserved IMR bits, GS revision/ID,
  FIFO status and full CSR RESET/FLUSH behavior are not fully implemented.
- Interlaced CSR.FIELD bit13 now alternates at VBLANK; progressive clears it.
  CSR W1C only clears interrupt status 0..4 and preserves read-only FIELD.
  Precise PAL/NTSC scan timing is still approximate.
- MTAP open: checked actual supplied SCPH-50004 XMTAPMAN export ordinal4,
  text0xC40..0xCD0. Both detected and absent adapter return1 for port<4;
  invalid port returns0. freemtap differs and is not the Sony contract here.
  Read port from preceding DMA send-buffer descriptor, NOT CALL+0x2c
  (which is recv_size). GetConnection explicitly returns0 (no adapter);
  Close returns1 for a valid port. No fake attached multitap.
- New PADMAN OPEN/CLOSE, INIT/END, port/slot counts and 128-byte double-buffer
  DMA layout, with frame counter and live digital port0/slot0 button data.
  Other physical ports and multitap slots stay disconnected. Mode changes,
  pressure and actuator commands remain unsupported. Old 64-byte protocol
  is separate. PAD registration state is not yet serialized in raw emulator
  checkpoints (existing old registration was also omitted).
- BC0F/T/FL/TL implemented from local PCSX2 COP0.cpp CPCOND0 and Dmac.h:
  ((D_STAT | ~D_PCR)&0x3ff)==0x3ff. Delay slots/likely annul checked.

## Tekken evidence / remaining frontier
All three local split ZIP parts are complete. Joined archive verified by ZIP
CRC; extracted raw BIN size728143920. SYSTEM.CNF identifies SLUS_200.01,
NTSC, stride2352/dataoffset24. Do not relabel it Europe Demo.
Fresh real BIOS boot reaches entry0x003572a0. Original R1254 parked after
32 BIND packets in allocator; current lifecycle fix gets through. Then actual
KExit exits at0x00396d54 (MTAP) and0x00396dc8 (PAD open) were eliminated.
FIELD fix advances from43 GIF QW/3 sprites to1454 QW/563 sprites around59M
EE, where missing BC0F at0x003430f8 used to halt. BC0 fix removes that halt.
Latest 600M fresh run remains active at job routines0x0035dff0/0x0035e1c8;
no visible Tekken picture. RAM job0x00415fc0 starts0000010101000000,
D_CTRL9/D_STAT00a60201/D_PCR200. Investigate genuine GIF/VIF job completion
and DMA interrupt timing, using a trace; do not force guest job counters.

## Builds / use
Diskless-Interpreter and Diskless-JIT ignore discs by design. For Tekken use
Disc-Interpreter and put the uncompressed BIN at sd:/pcsx2/games/game.bin.
Keep the user's existing BIOS setup. CHD decompression is not implemented.
Source/build script tools/build_r1255.sh uses distinct object directories.
Raw emulator checkpoints are host-ABI/revision dependent; new GIF fields
change that ABI. Deliverable checkpoint is source/build/test/handoff archive,
not a portable RAM save state. No BIOS/game/ROM payloads included.

## Next work
Inspect actual diskless render/boot transition and late framebuffer after
cold boot. Menu remains absent; do not mistake blue startup or bottom symbols
for Browser/System Configuration. Then continue Tekken job/DMA frontier.
Only after actual menu verification, profile and improve JIT/performance.

## Final verification for this checkpoint
157/157 host-native regression tests passed (suite_results.json).
Actual big-endian PowerPC ELF: 53 GS color/depth checks +19 CLUT checks passed.
Fresh diskless 1200M with FIELD/MTAP/PAD fixes: HALT=0; displayed framebuffer
word base163840 remains black except1166 bottom-button pixels. Alternate
framebuffer491520 contains9481 blue-orb animation pixels; no menu text.
Fresh 1500M IMR control also HALT=0, no menu. These native cold boots are
not Dolphin execution and do not establish a Wii FPS improvement.
