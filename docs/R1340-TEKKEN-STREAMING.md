# R1340: verified retail RSPU2 transport

Tekken Tag Tournament SLUS-20001 binds the ordinary SPU2 SID but uses a
retail extension, not ps2sdk's ordinary sound RPC layout. The provider is
accepted only after its complete 66,077-byte disc image matches FNV1a
0xe16bba10. Nothing changes for another provider or a BIOS-only session.
No copyrighted module/table/disc/ROM bytes are distributed.

## Implemented

- 0x2000 initialization validates the retail 0x2237 key. Duplicate valid
  initialization is idempotent. Cold boot and accepted IOP reboot clear it.
- 0x2030 reports actual HLE resource/stream state: resource activity bit 0,
  read error bit 2, streaming-transfer completion bit 5, idle bit 6.
  It is not an unconditional 0x40 success reply.
- Existing 0x204e EE and 0x2045 SPU2 RAM resource reads retain bounds,
  genuine disc failures and EE source invalidation; they update read status.
- 0x2050/0x2051 preserve the idle synchronous resource service contract.
- 0x2058 initializes bounded physical-sector and video-sector cursors. Film
  and associated audio byte counts are read from the verified provider,
  never embedded as a guessed title-specific size.
- 0x2059 reads real sectors into EE RAM bus addresses, invalidates written
  native-source pages, and reports completion only after the requested
  video sectors have actually been copied. No synthetic EOF bytes. Read
  failure leaves the successful prefix visible and retains the next unread
  sector for a bounded retry.
- Audio/video group geometry comes from 0x2058 itself. This disc requests
  seven audio sectors followed by 128 video sectors. The audio prefix is
  staged separately; only the video suffix enters the EE MPEG ring. Both
  geometry and partially consumed group state survive checkpoint/resume.
- 0x205a stop/idle and 0x205e packed sound configuration (48 bounded
  hardware voice slots) are represented. Unknown commands remain with the
  existing generic provider path and do not acquire invented semantics.
- Optional RSPU checkpoint block includes provider configuration, cursors,
  raw audio staging, sound parameters and completion/error state. Legacy
  snapshots without that block start with disconnected/uninitialized state.
  Fresh cold boot is required for meaningful comparison with old releases.
- RSPU2_STREAM logs initialization, last command, status, physical reads,
  transferred video sectors, remaining physical sectors, errors and audio
  sectors. Polling 0x2030 does not print on every call; errors are rate-limited.

This is synchronous HLE transport, not a new IOP dynarec or execution of the
IRX's worker threads. It adds no per-instruction polling to the CPU scheduler.
Raw streaming audio synthesis/output is still separate: the latest ADPCM
prefix is retained, but configuring/staging it does not prove audible output.
Existing SPU2 asset transfers and the existing mixer remain available.
The R1339 ARM CSC loader fix is retained; no sound/cache/EE work is moved to
ARM by this release, and physical ARM acceleration is not claimed.

## Verification and private boot

Synthetic ASan/UBSan tests cover initialization, state-derived status, real
byte copies, RAM bounds, invalidation, partial read error, retry, EOF,
interleaved audio/video extraction and snapshot validation. Native checkpoint
round-trip and malformed/duplicate/legacy behavior are checked. The actual
linked big-endian PPC transport is tested with mocked sector I/O and libc;
status, copies, prefix stripping and invalidation execute the Wii ELF.

The private cold Tekken boot leaves the former 900-frame 0x2030 idle wait and
starts real 0x2058/0x2059 transfers. An intermediate run before audio/video
separation reached FDEC (0x40000000) but starved the IPU; it is not proof of
macroblock decoding or an FMV picture. The corrected native cold run reaches a real Tekken Tag title graphic in
the 640x224 GS display viewport by EE 1,199,959,619: GIF 309475 QWC,
IPU accepted 36520 QWC, seven BDEC and 14 VDEC commands. It transfers 288
video sectors and separately stages 21 audio sectors without HLE transport
failures. The guest stops the stream and displays the title graphic.
This is not a complete FMV: only a small prefix of the film was transferred;
no interactive title navigation or physical Wii image is verified. The
native test executes the interpreter, not PPC JIT.

Do not describe this as a verified Tekken title screen, complete MPEG player,
full sound synthesis, physical Wii speedup or completed PS2 emulation without
new runtime evidence. Native boot cannot execute the Wii's PPC dynarec and is
used for compatibility investigation, not physical performance measurement.
