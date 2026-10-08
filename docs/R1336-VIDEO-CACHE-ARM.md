# R1336 — IPU access, cache publication and video worker

Physical Wii is the primary target. This release fixes two reproduced CPU/cache
regressions, adds real VDEC/PACK processing, and ships a separately buildable
experimental ARM IOS service. It does not claim a measured Wii FPS increase,
complete MPEG playback, or a KOF/Tekken title screen.

## Boot regression and executable cache

Owner R1335 BIOS logs stop before GS/GIF/VU work: SETIQ is BUSY with no accepted
IPU input. Scalar PPC SQ/LQ previously decomposed MMIO into two 64-bit accesses;
the interpreter used an atomic 128-bit FIFO access. R1336 makes both paths call
the same guest-byte-order quadword helpers. Load/base aliasing is preserved.
The regression test fails on R1335 and passes on R1336 with Fastmem ON/OFF.

Owner R1334 logs show 3.3–4.0 million code-arena allocation failures despite
roughly 1.9 MiB live in the 6-MiB arena. Compilation reserved a worst-case body
before trimming it. R1336 compiles into an unpublished scratch buffer and
allocates only the emitted size in the arena. External relative calls are
rebased before publication, then the normal PPC data/instruction cache flushes
run. Published/executing blocks never move. A fragmented-arena test with about
4 MiB free fails on R1335 and passes on R1336; existing owners remain intact.

The executable arena already lives in PPC-addressable RAM. Moving it to ARM
would not allow ARM to execute PPC instructions or remove PPC cache maintenance.
This release keeps the arena local. Its tests demonstrate correct allocation,
not shorter physical-Wii compilation times or a particular FPS improvement.

## Video commands

* VDEC: macroblock address increments, escapes/stuffing, picture-type modes,
  signed motion codes and differential motion vectors. Consumed bits and TOP
  refill are separate phases; a refill or checkpoint cannot decode twice.
* PACK: RGB32 to dithered RGB16 or nearest VQ palette index, with explicit guest
  byte order and real input/output FIFO backpressure and DMA completion.
* Bit extraction uses byte spans instead of one loop iteration per bit.
* Existing CSC, FDEC, SETIQ, SETVQ and SETTH remain available.
* IDEC/BDEC still use the explicitly counted legacy unsupported path. There is
  no complete MPEG coefficient decode/IDCT/picture reconstruction yet.

VLC tables/semantics and PACK/dither follow primary PCSX2 source at
[3c8df07](https://github.com/PCSX2/pcsx2/tree/3c8df07e8e367caef8386fd949d6ca44bf2f7ad0/pcsx2/IPU).
Original PCSX2/libmpeg2 notices are retained in the adapted source.

## ARM service

`arm/ios_service.c` registers `/dev/gekko2`, receives IOS IPC requests, validates
the capability query and CSC vector spans/counts, runs the portable CSC kernel,
flushes output and acknowledges completion. Shared spans cannot wrap or overlap.
The service accepts 1–8 macroblocks; the current PPC client submits one at a time.
The client remains asynchronous and rejects stale results across cold boots.
Guest state, JIT owners and mutable guest RAM are not sent to the worker.

Build with `DEVKITARM` or `ZIG` using `sh arm/build-worker.sh`. The resulting
`arm/build/Gekko2-ARM-Worker.elf` is freestanding ARM926, big endian, soft float.
The default experimental base is `0x137f0000`, configurable with
`ARM_WORKER_BASE`. A compatible loader must reserve every ELF load span, zero
BSS, provide the declared stack and start its IOS thread. The DOL includes no
loader, IOS reload, NAND installer or firmware patch. Standard IOS does not gain
this service merely by enabling the option. `ARM_WORKER available=0` means CPU
fallback; `submitted/completed` only indicate worker jobs when a service exists.

IOS ABI and syscall wrappers were checked against
[d2x-cios](https://github.com/wiidev/d2x-cios/tree/master/source/cios-lib).
The worker ELF executes successfully in an ARM CPU test with mocked IOS kernel
and cache calls. Resource-manager queue/open/caps/CSC, 1/3/8-block jobs, RGB16
dither and invalid requests are covered. Physical-Wii loading, IPC cost and
throughput are unverified. No ARM acceleration is enabled by default.

## KOF diagnosis

The supplied European disc reaches two independent boot defects: legacy
LOADFILE protocol query 255 returned zero, and SetupHeap was acknowledged as a
no-op. R1336 reads the actual constant getter from the selected LOADFILE module
(bounded ELF parsing, no game-name assumption) and routes SetupHeap to the
selected guest BIOS kernel, as SetupThread/EndOfHeap already do.

With those fixes, a native cold disc boot passes module initialization and heap
allocation, runs the game program and configures GS PMODE/GIF. It next polls for
an asynchronous CRI/CDVD streaming response. That run issues no IDEC/BDEC/VDEC
commands. It is not evidence that MPEG decoding caused this current wait, nor
that a movie/title screen renders. Proprietary CRI streaming and real loaded-IOP
module execution remain work. No guest PC, semaphore or memory flag was patched
to force progress. Private BIOS, ISO, module dumps, RAM and runtime logs are not
included in the source or release.

## Wii test

Use Optimized first. GX OFF, Fastmem OFF, cache reuse ON, HLE RAM ON, strict HLE
OFF, ARM OFF. Use COLD BOOT / NEW LOG, not pause/resume, to apply changed options.
Then cold boot with Fastmem ON, otherwise identical. Compare first image and
IPU accepted/completed counts plus code-arena failures; EE totals alone do not
show useful boot progress. Optimized defaults reuse ON, Control OFF; saved
settings take priority. Both contain all fixes and support GX/Fastmem.

EE compact cache remains the independent 4096/1024-owner comparison switch;
it is not required for the quadword or exact-allocation fixes. Existing native
checkpoints from older struct layouts should be regenerated by cold boot.

## Validation

See release `Verification.json` for the final linked-PPC/native/ARM results.
PPC CPU tests execute the actual Wii ELF while mocking libogc transport/GX;
native tests do not execute PPC JIT code. Decoder ASan/UBSan checks use synthetic
streams, all 16-bit VLC prefixes/picture types, every FIFO bit offset, checkpoint
resume and output backpressure. LeakSanitizer is disabled due to unavailable
process tracing in the test environment. DOL sections match their ELF bytes and
MEM1/entry layout. Hardware boot/FPS and complete game compatibility remain
unverified.
