# R1351C Beta: DSI in the precise EE block path

User Wii log25 is R1351A; log26 is R1351B. Both use mask01fd7fff.
A first image:408974ms. B:410721ms. B passed the actual Wii MMU/self-test,
status1/probes4, but emitted0 sites. The scalar LW emitter was not the
execution route used there. These are observed A/B results, not C results.

C adds a separate LW body to precise full/prefix/delay blocks. The boundary
callback still verifies the live source word, instruction state, alignment,
delay context, retirement and scheduler boundary. For aligned kernel-segment
LW with active DSI/Fastmem/arena it returns physical offset+1 without the old
software RAM-range classification. The host MMU distinguishes RAM from
MMIO/ROM. Guest-TLB reads retain the original RAM proof before the body;
unaligned accesses and stores retain their old admission/fallback paths.

The emitted body reads through the mapped window and contains relative DSI
metadata. A fault returns through the existing integer-only exception glue
to an in-owner branch into ee_mem_read32. The original virtual address is
recomputed before writing rt, including rs=rt and rt=0. The fallback preserves
the outer block context/budget/LR and restores r3 as the state pointer.
Memory helpers run in normal context. Source/mapping invalidation is unchanged.

Word allocation is excluded for this body, because rewriting skipped branch
metadata would corrupt the handler's descriptor. Register residency fences
the branched body and invalidates its cached word knowledge; it is not globally
disabled. Event windows still require nonfaulting RAM proofs and retain their
old load body; speculative MMIO must never see prematurely published clocks.
Fetched-first admission also retains the old proof; subsequent precise
boundaries and delay slots gain DSI classification.

New DSI_PROGRESS/DSI_FASTMEM fields:
- block_sites: translated DSI block-LW sites (includes compilation attempts).
- block_prepares: eligible precise KSEG LW preparation attempts; not a retired
  instruction count, and a later preparation guard may still decline.
- emitted: scalar plus block sites.
- faults/patched: recognized site faults/backpatches, excluding startup probes.

Use the same settings as B, quit/relaunch C and Cold Boot. EE dynarec,
Fastmem and6MiB arena remain ON. No extra DSI option or IOS installation.
Expect status1/probes4 and positive block_sites/block_prepares during BIOS boot.
faults=0 alone is not failure: mapped RAM does not cause DSI. Record the whole
log and time to FIRST_IMAGE; no speed claim exists yet for physical C.

Verification executes the shipped PPC blocks, boundary callbacks, memory
helpers, allocator and exception glue with modeled privileged MMU/SPR delivery.
The actual CPU entry now generates block sites, reads RAM and MMIO, preserves
discarded-read side effects and patches MMIO sites. Complete guest/timer states
are compared with the guarded path for dependencies, aliasing, short grants,
branch-delay MMIO, unaligned accesses and guest-TLB faults. Platform-model
tests are not real Wii exception/cache timing measurements. B's real-Wii
self-test validates mapping/probe setup but did not validate JIT backpatches.

Toolchain unchanged: devkitPPC r50/GCC16.1.0/libogc3.1.0. ARM reply corruption,
zero vertices and complete JIT coverage remain separate open work.
