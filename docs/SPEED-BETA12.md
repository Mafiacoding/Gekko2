# R1389 SPEED BETA12: Claude BETA11 integration

Base: R1389 commit 19069caf69d51240b956a03b2ecd067dc0aa739d.
Claude source_all.patch and tools_all.patch are imported in full, without fuzz.
Import commit c22ea71 includes the missing quiet-stub type/prototypes reconstructed
from the supplied implementation; the uploaded ZIP omitted header changes.
The original BETA11 DOL and the prior R1389 build remain separate.

## Scheduler and speed

BETA11 rescans current IOP alarm/delay deadlines when a hint is invalid or its
checkpoint blob is exposed. This avoids pessimistically restricting EE grants
to one IOP tick. EE event windows, quiet PPC retirement, RAM-load translation
proofs and idle/spin skipping are retained. The user's 0.31 Tekken FPS is a Wii
observation for Claude's build, not a measured BETA12 result. Handoff timings
use different cold/warm runs and do not establish a controlled speed ratio.
Register-hash spin detection remains an experimental heuristic: unchanged
registers alone do not prove the absence of memory/MMIO side effects.

## Additional fixes

- Complete omitted EE/IOP/PPC function interfaces so the source builds.
- Make GEKKO2_FORCE_EVENT enable host scheduler quanta too; without the missing
  header update, the handoff's host flags silently retained legacy interleave.
- Host object caches now include compiler flags, preventing stale objects when
  changing HOSTEXTRA between scheduler modes.
- Cold boot clears quiet budgets, cached TLB proofs, spin history and counters.
  Mapping invalidation also revokes outstanding quiet budgets.
- General register-hash skipping excludes BIOS/kernel and exception contexts.
- IOP spin skipping requires registered THREADMAN and no armed alarm/delay
  deadline; otherwise its unchanged instruction clock could starve these events.
  RAM proof bounds reject wrapped addresses instead of risking host dereferences.
- Split MPG/UNPACK completion with I-bit now retains subsequent VIF words until
  FBRST.STC; previously it continued parsing despite raising an interrupt stall.
- Optional GEKKO2_DMA_TRACE in the host driver dumps all ten DMA channels and
  VIF state; no guest flags are forcibly cleared.

## Validation and limits

New tests exercise uncached/exposed IOP deadlines, split-transfer IRQ ordering,
IOP spin guards and cold reset. 11 host test programs pass; the linked PPC
suite passes 120 full EE/timer/display/IRQ comparisons with 123 selected
successors, plus three IRQ context cases. The focused ALU suite passes 18
comparisons. These are instruction/state tests with mocked platform calls,
not Wii execution or physical FPS. Existing no-card MCSERV, MFIFO producer, 12,000 IOP deadline
traces, IRQ context, VU1 reset and R1384 hardware tests pass.
The general VIF suite passes after its two V4 payload fixtures explicitly set
CL=WL=1. Their previous reset CL=0 assumptions incorrectly selected fill mode;
the production reset/cycle implementation was preserved.

The owner-only 460M-IOP Tekken checkpoint still reproduces black output, unchanged
GIF/VU counters, VIF1 IRQ stall and 63 buffered words, with an active VIF1 DMA
REF payload QWC 0x958d. That state is already stalled before BETA12's split IRQ
fix. This identifies a frontier, not a confirmed cause or fixed battle screen.
The 340M checkpoint replay did not establish a new battle scene either.
An early optional host diagnostic was discarded: an unchecked guest pointer
read could raise its own TLB exception. Diagnostics now inspect raw state only.
A corrected BETA12 cold boot reaches executable RAM and rendering again;
this is boot evidence, not proof of battle-screen repair.
The complete owner Tekken image is 728,143,920 bytes, ZIP CRC checked; earlier
partial-extraction runs were discarded. Full-image replay reaches both selections
and VS, then reproduces the VIF1 stall. A live watch identifies the malformed
REF tag (0x3000fff3) written to scratchpad by guest packet-building code; its
caller/source length is still being traced. The controller plan performs genuine
guest input, without patching its PC, semaphore or DMA busy flags.

All 16 owner game.part volumes were recovered and checked during libarchive
extraction: GT3 PAL SCES_502.94, 3,687,022,592 ISO bytes. A fresh event-scheduler
boot reproduces the genuine Copyright screen; repeated modern GETINFO calls
alternate card ports 0/1, slot 0. No later scene or race is verified yet.
No KOF image was identified. Inserted-card filesystem support remains a known MCSERV gap;
its success-without-output branch is not claimed fixed. No race or memory-card
repair is claimed. Fresh boot results and private snapshots stay separate from
public source. Wii execution, native GX pixels and BETA12 FPS remain unverified.

## Build and apply

Use tools/build_speed_beta12.sh with the tested devkitPPC r50/GCC16.1.0,
libogc3.1.0 SDK. Public generated AOT tables are empty, as in the uploaded plain
source baseline; no guest BIOS/game words are shipped.
The combined R1389-to-BETA12 patch targets the clean R1389 source above.
The BETA11-to-BETA12 patch targets import c22ea71 (with omitted interface restored).
Use git apply --check before applying. Do not apply both patches in sequence.
