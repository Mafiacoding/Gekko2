# R1337 — coefficient decoder, arena bins and ARM RAM loader

R1336 is the reference release. Owner testing reports quicker Wii BIOS boot
and 0.09–0.11 FPS versus 0.03 in earlier releases. The supplied all-options
R1336 log records FIRST_IMAGE at 572963 ms / 295412641 EE instructions,
no arena allocation failures, and unavailable ARM service. These observations
are not a physical-Wii measurement of R1337.

## Decoder

BDEC now reconstructs six MPEG coefficient blocks through DC/AC VLC tables,
escapes, inverse quantization and scalar IDCT, yielding real 768-byte signed
16-bit residual or intra YUV macroblocks. Normal/alternate scan and interlaced
block layout are supported. IDEC parses intra macroblock types, quantizer
changes and address increments, reconstructs macroblocks, then performs CSC,
SGN and RGB32/RGB16 output. Start-code/TOP completion, input starvation,
output FIFO backpressure and resumable decoder state are retained.

The tables and IDCT preserve PCSX2/libmpeg2 credits and are adapted from PCSX2
revision 3c8df07e8e367caef8386fd949d6ca44bf2f7ad0. These are IPU commands,
not a complete movie/container/audio subsystem or a replacement for guest
motion compensation. Invalid input takes the IPU error path; no guest PC,
semaphore or fabricated frame is injected.

Native sanitizer tests exercise BDEC/IDEC, signed zero residuals, legal IDCT
extrema, two-macroblock IDEC, SGN, RGB16/32, FIFO stalls and checkpoint resume.
Six independently FFmpeg-encoded synthetic MPEG2 intra macroblocks (gradient,
flat and checker patterns, two quantizers) match FFmpeg's decoded 384-byte YUV
output within one intensity unit. Actual linked Wii PPC tests also execute
BDEC/IDEC and verify their guest-byte-order output. Broader real streams,
MPEG1 escapes, alternate scans and movie playback still need hardware coverage.

## Executable arena

The six-MiB PPC code arena remains in PPC-addressable RAM. R1336's unpublished
translation scratch, exact-size publication and external BL rebasing remain.
R1337 replaces the address-sorted free list with 18 size classes and a bitmap;
physical boundary tags provide constant-time neighbour coalescing. Published
owners never move. A 100000-operation randomized sanitizer test preserves all
live bytes across allocate/release/trim and restores the full contiguous arena.

In a synthetic linked-PPC allocation test with 256 irrelevant small holes,
32 larger requests plus releases take 143636 PPC instructions on R1336 and
7302 on R1337. This is about 20 times less allocator work in that case; it
does not establish a Wii FPS gain or a 20-times-faster JIT overall.

The supplied R1336 log's exclusive CPU samples allocate about 48% to EE, 22%
to scheduler, 13% to translation and 12% to IOP. IOP already executes 67228118
native instructions versus 30 interpreter instructions. EE budget variants
still generate substantial translation work (2253298 variant installs);
this release does not eliminate those variants or change precise scheduling.

## ARM support and installation

The ARM926 big-endian IOS resource manager from R1336 remains `/dev/gekko2`.
It executes the portable CSC kernel; the PPC client uses private, aligned
buffers and rejects stale cold-boot results. A completed job now wakes IPU
service at the next frontend timeslice even when the guest only waits for an
IRQ. There is no per-instruction ARM polling in the EE/IOP hot loop.

R1337 adds a bounded MLOAD RAM loader. When ARM is enabled, an existing
`/dev/gekko2` is used first. Otherwise the DOL looks for:

`sd:/pcsx2/arm/Gekko2-ARM-Worker.elf`

Copy the release's `arm/Gekko2-ARM-Worker.elf` there, enable ARM, then restart
the emulator and use cold boot. The current IOS must already provide a
compatible `/dev/mload` and authorize the ELF load spans through GET_LOAD_BASE.
The loader validates ELF32 big-endian ARM, segment bounds/non-overlap, entry,
IOS metadata and stack. It checks every target span is zero before any write,
loads code/data with BSS zeroing, then starts the declared thread using MLOAD.
It does not install, patch or reload IOS, write NAND, or overwrite occupied
target spans. MLOAD operations follow Hermes' interface in
wiidev/usbloadergx/source/mload; original upstream notices are retained.

Standard IOS normally lacks MLOAD. Enabling ARM alone cannot provide kernel
permission to load a service. That remains CPU fallback, not ARM acceleration.
The loader is experimental: no cross-process reservation is provided by MLOAD;
do not run another module loader concurrently. Hardware loading, syscall
compatibility, IPC latency and throughput are unverified. Cache/JIT code and
guest mutable RAM stay on PPC. ARM currently offloads CSC, not EE/IOP/VU or
the full coefficient decoder/IDCT.

`ARM_LOADER status`: 0 not attempted, 1 thread started, -1 no ELF, -2 file
read/size/allocation, -3 no MLOAD, -4 IPC heap, -5 load-region query, -6 invalid
image/outside authorized region, -7 target read failure, -8 occupied target,
-9 write failure, -10 thread start failure. After starting, capability probing
still determines `ARM_WORKER available`. Positive loader status alone is not
proof that a usable service registered. Restart after correcting a loader
problem; the client avoids repeated load attempts during a boot session.

Tests execute actual ARM service code with mocked IOS syscalls, and native
sanitizer tests cover loader success plus absent MLOAD, short reads/writes,
small authorized areas, occupied second-segment rejection and malformed ELF.
These checks do not demonstrate physical-Wii installation or performance.

## Wii comparison

Use R1337 Optimized, retain the same settings as the supplied R1336 run and
cold boot/new log. Compare FIRST_IMAGE time, CODE_ARENA failures and translation
counters at the same visible BIOS phase. Then test ARM separately with the
worker ELF installed; inspect loader status, available and submitted/completed.
If no service is available, that run remains a CPU comparison. BIOS startup
issues no CSC/IDEC/BDEC in the supplied log, so ARM CSC alone cannot accelerate
that boot phase. Do not infer an IOP bottleneck from its instruction counter.

KOF's known CRI/CDVD streaming wait remains unresolved. This release does not
claim a KOF/Tekken title screen. No BIOS, game, SDK or private runtime data is
included. Optimized/Control differ only in default cache reuse; saved options
take priority. Older native checkpoints must be regenerated by cold boot.
