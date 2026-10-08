# R1334: EE owner capacity, boot resets and game diagnostics

Wii is the primary target. This checkpoint changes CPU metadata and adds
observations of the existing IPU skeleton. It does not complete the IPU decoder,
change GX rendering, relax CPU correctness checks or add an ARM worker.

## Evidence from the owner's R1333 Wii logs

| BIOS run | First image host time | EE instructions | IOP instructions |
| --- | ---: | ---: | ---: |
| Software, Fastmem off, session 1 | 856801 ms | 295423531 | 44908185 |
| GX/Fastmem on, same-process cold boot, session 2 | 674659 ms | 295403827 | 3822444 |
| GX/Fastmem on, fresh app start, session 3 | 759601 ms | 295397083 | 44904879 |

The fresh GX run is about 11.34% shorter than the software run, at almost the
same EE count. These are individual owner runs with more than one option
changed, not a controlled measurement of GX or Fastmem alone. The 21% decrease
in session 2 has a materially different IOP trajectory and is not attributable
to those switches. In the late 20 samples of session 3, EE translation accounts
for about 31.48% of sampled CPU time, EE execution 40.67%, IOP 9.50%, scheduler
15.98%, idle 2.37%, GS raster approximately 0.01%. Sampling is not a complete
per-operation timing trace. Session 3 installed 4414158 EE blocks and counted
3268146 collisions; arena high water 990048 bytes of 6291456, no arena failures.
GX accepted 26 quads, including hardware depth/blend; it was not universally
unused. Degenerate triangle counts are not a substantive Gouraud workload.

## EE block-cache change

Block cache reuse ON now keeps 4096 owners (1024 sets, four ways), versus
R1333's 1024 owners (256 sets, four ways). OFF retains 256 direct-mapped owners.
Owners contain source words, generation guards and learned-edge identities;
they are separate from the unchanged 6-MiB executable code arena. Dispatch
metadata still occupies one aligned 32-byte Broadway cache line per owner.
The additional static metadata is 332544 bytes, verified from the ELF symbols.
The maximum live translated code footprint can grow; existing bounded arena
allocation/failure behavior remains authoritative.

All lookups, budget-length variants, link-index bounds and reset iteration
use the new layout. Reset releases occupied entries before bulk-clearing the
tables, avoiding redundant per-entry clears of empty slots. Live source/mapping checks, pinning of executing native
allocations, retirement checks and EE8/IOP1 interleave remain unchanged.
EE_CACHE_LAYOUT logs the active entries/sets/ways. No new menu switch is needed.

## Directed linked-PPC replay

128 grants, identical guest instructions/state and mocked platform services:

| Workload | R1333 installs | R1334 installs | R1333 PPC instructions | R1334 PPC instructions |
| --- | ---: | ---: | ---: | ---: |
| Single warm PC, budget 8 | 1 | 1 | 513691 | 515488 |
| Eight PCs colliding in the old set, budget 8 | 128 | 8 | 4466645 | 734939 |
| Eight old-set PCs, alternating 8/2 grants | 128 | 16 | 2704153 | 628078 |

These counts match the final ELF replay in the package.
Warm-case cost rises about 0.35%; directed collision replay drops about 83.55%
and budget-variant replay about 76.77%. This is a deliberately collision-heavy
synthetic test; it is **not a measured Wii FPS or BIOS boot-time gain**. Physical
write notification and subsequent execution check both block lengths.

## Boot-local IOP state

IOP initialization now also clears spurious I_STAT acknowledgement masks,
zero-run diagnostic state, stale-block counters and the before-tick callback.
A repeated-init host test and interrupt regressions verify reset behavior.
These were concrete missing resets. They do **not establish the cause** of
session 2's parked IOP or prove a BIOS/cold-boot compatibility fix.

## Game/IPU audit

The Paris compatibility work is retained: GS DECAL/TCC alpha routing, TLB
invalid/modified exceptions and mapping invalidation, GS SIGNAL/FINISH,
VIF IRQ stall/resume and prior loader/DMA changes. Tekken Tag Tournament has
reached the Namco logo in native work and owner Wii tests; title/gameplay and
playable performance remain unverified.

Current source/hw/ipu.c accepts registers and command acknowledgements, but
has no MPEG/VLC/IDCT output implementation, output FIFO delivery, FROMIPU DMA
producer or correct input backpressure. Even its accepted input is discarded.
This can block real game movies or other uses of IPU commands. It is a
compatibility gap, not established as the bottleneck in the supplied BIOS logs.

R1334 adds per-cold-boot IPU_STATUS and IPU_COMMANDS observations. They report
all 16 command codes, unimplemented commands, submitted/accepted/discarded QWC,
FIFO count and last command. output_available=0 explicitly identifies the
existing skeleton. Diagnostic reads preserve guest-visible state; in-game IPU
reset preserves session observations, while ipu_init on cold boot clears them.
No fake output or simulated decoder success is introduced.

Next compatibility pass: run the existing BOOT DISC path with Tekken and an
owner-selected simple 2D game, log the earliest stall and whether it uses IPU.
Loading a game ELF avoids the OSDSYS menu animation, but it still requires the
implemented kernel/IOP/disc/DMA/graphics environment. If the failing scene uses
IPU, implement/test actual FIFO bytes and backpressure, output DMA and command
completion/IRQ before MPEG command/decoder coverage. Choose priority from the
first failing command/path; no claim that IPU completion alone enables gameplay.

ARM is a separate future area: [feasibility and measurement gate](R1334-ARM-ROADMAP.md).

## Builds and verification

Optimized defaults cache reuse ON; Control defaults OFF. Saved SD settings
can override either; both can use the same switches and the same renderer.
Fastmem remains experimental and OFF by default. Compare one setting at a time.
Cold Boot applies pending options; START/PLUS only pause/resume.

Validation: 26/26 linked PPC suites, six focused host CPU/IOP/log tests,
IPU register/reset observations under ASan/UBSan (LeakSanitizer disabled
because this sandbox cannot inspect its task directory), both cross-builds
and DOL/ELF section checks. Control also passes session/capacity replay.
See package Verification.json and tools/verify_r1334.py for exact results.
Linked PPC verification executes generated code under Unicorn with mocked
platform/GX services. DOL/ELF byte checks are not a Wii/Dolphin boot test.
No physical-Wii R1334 result, ARM offload, MPEG playback or gameplay is certified.
