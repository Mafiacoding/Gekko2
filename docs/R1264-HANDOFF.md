# R1264: OSDSYS reboot configuration and libmc initialization

## Root cause and change

A private read-only trace of a fresh SCPH-50004 boot observed OSDSYS sending
SIF RESET_CMD (0x80000003) at EE=49,959,580: packet size 104, arglen 21,
mode 0, argument `rom0:UDNL rom0:OSDCNF`. The existing EE SifSetDma handler
copied this packet to IOP RAM but ignored its reboot/configuration semantics.
Its initial IOPBTCONF loader state had 29 entries and no MC modules, and no
mcman/mcserv image names were found in that checkpoint's IOP RAM.

OSDSYS then called MCSERV INIT function 0xfe at EE=49,969,578. The existing
cardless RPC HLE returned zero module versions. The actual guest libmc
requires MCSERV >=0x205 / MCMAN >=0x206; it cleared client.server and later
mcGetInfo returned -100 without reaching the provider. This is the blocker
identified in R1263, not evidence that all Browser Back behavior is explained.

The new reset-packet consumer updates the existing RPC HLE's provider metadata
from the actual requested configuration. OSDCNF is a nested ROM image; its
IOPBTCONF selects XMCSERV and XMCMAN in the outer BIOS ROM. Their real
SHT_MIPS_IOPMOD metadata is MCSERV 0x208 / mcman_cex 0x209. The name matcher
now recognizes mcman_cex alongside mcman/mcman_tool. No global highest-version
selection, fabricated version constant or forced guest client.server is used.

The fixed 104-byte reset header, packet argument lengths and nested ROM extents are checked. Malformed or
truncated argument packets do not replace valid provider metadata. A valid
reboot to an unsupported/missing configuration or mode clears old versions,
so stale providers cannot appear to belong to a new boot. Only the supported
mode-0 `rom0:UDNL rom0:<config>` request is resolved at present.

## Scope

This supplies truthful metadata to the fork's already-existing cardless
MCSERV HLE. It does NOT execute selected IRX entry points, replace the running
IOP module-loader list, reset all IOP peripherals/threads, or port the full
IOP reboot protocol. Other provider and filesystem behaviors remain partial.
The new routine intentionally says note_mc_config rather than full reset.
Existing R1263 display-clock, modern OPEN and checkpoint behavior remains.

## Validation

- Native suite: 171 / 171 tests PASS. The new integration fixture sends a real
  reboot DMA followed by MCSERV INIT against a synthetic nested ROM. It tests
  selected modern providers versus an unselected higher-version legacy image,
  mcman_cex, INIT reply boundaries, malformed/truncated args, missing config,
  unsupported mode and overflowing nested config extent.
- Both Wii ELF/DOL variants build successfully.
- Actual emitted Wii ELF executed under Unicorn: 167 checks per Interpreter
  and JIT variant PASS, including 18 new reset/config/INIT checks. These test
  the shared interpreter/RPC core; generated JIT blocks are not covered.
- Fresh native BIOS boot early checkpoint at EE=51,198,617: tracked versions
  0x208/0x209, actual guest libmc.server=0x1000, retained reply versions
  0x208/0x209, and the next actual GETINFO reply is -11 (no card). HALT=0.
  The read-only tools/verify_osdsys_mc_r1264.c fixture passes all five checks.
  Its RAM addresses deliberately target the supplied SCPH-50004 OSDSYS and
  are inspection-only, not production guest-state patches.

## Full native coldboot and Browser Back evidence

The controller-only coldboot reaches the actual main menu after setup:
EE=1,484,733,065, PC=0x00205118, HALT=0. Active GS source region is
640x256, decoded to 640x448, BP=0 / BW=640. The orb remains fragmented.

CROSS for 15M retired EE instructions, release for 60M reaches the actual
Browser No data / Back display at EE=1,560,057,300, PC=0x00263fb0, HALT=0.
From that saved Browser state, CIRCLE for 15M followed by release for 30M
returns to Browser / System Configuration at EE=1,605,551,220,
PC=0x0026d7dc, HALT=0. See all three active-display PNGs and navigation logs.
A second complete CROSS/release/CIRCLE/release cycle from the returned menu
also returns to the actual main menu: EE=1,726,402,041, PC=0x00235284,
HALT=0. The repeat navigation and active-display inspection logs are included.

No guest PC, initialization flags, module-version constants or client.server
RAM was forced by the test drivers. This fixes the previously reproduced
black Browser Back transition for this tested BIOS/controller sequence;
it is not a claim of complete OSDSYS or memory-card filesystem support.

The full coldboot started with the first R1264 implementation. Final code
adds strict 104-byte reset-header validation to avoid misinterpreting an
unrelated RPC payload as a reboot. A second genuinely fresh boot of the final
code reaches the exact same early checkpoint field state across 33 blocks,
including byte-identical EE and IOP RAM. Only four process-local RAM/BIOS
pointers and six unused ICDV native ABI tail-padding bytes are excluded from
that comparison. The final core was used for both Browser and Back tests.
Native tests and both emitted-ELF checks were rerun after header validation.

A separate read-only short menu draw trace records textured STQ triangles,
untextured lines and textured UV sprites; textured LINE support was not
exercised there. Do not label missing textured lines as the orb's cause.
A private GS Q-lifecycle experiment separated PACKED temporary Q from the
latched RGBAQ.Q, applied it on PACKED RGBA and reset it at nonempty tags,
following the bundled primary GSState.cpp behavior. Starting from the same
actual menu checkpoint, both sampled frames after 3.196M / 6.392M retired EE
instructions were pixel-identical to the production baseline. It did not fix
the orb and was not merged. This is a short negative result, not evidence
that all Q-lifecycle semantics are already correct. Existing ICDV checkpoint tail padding is not initialized; a
future cleanup should make serialized padding deterministic.


## Reproduction and packaging

Build: tools/build_r1264.sh with the supplied devkitPPC/libogc setup.
Native regression: tools/verify_regressions.py.
PowerPC: tools/verify_ppc_mc_bootconfig_r1264.py ELF --nm powerpc-eabi-nm.
Coldboot: tools/verify_osdsys_setup_r1264.c BIOS fresh checkpoint-out 39
 optional-early-checkpoint-out. The optional early snapshot is taken after
 51M retired instructions without editing guest state.
Navigation: tools/verify_osdsys_browser_r1264.c with an actual menu checkpoint;
enter-only and back-only modes allow separate Browser and return evidence.

The downloadable checkpoint is a source/build/log ZIP, not execution RAM.
No BIOS, game images, IRX data, raw guest memory or private firmware
instruction traces are included. Cumulative patch and diff target the original
pcsx2-wii-full-debug(1).zip source; exact patch apply and ZIP CRC are checked.
Full EE/IOP/VU0/VU1 JIT, persistent NVRAM and orb rendering remain pending.
There is no physical Wii/Dolphin measurement or supported 10 FPS claim.
