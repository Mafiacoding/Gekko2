# R1265: real OSDSYS configuration storage

## Change

The pre-R1265 SCMD configuration family acknowledged writes without storing
anything and supplied zero-filled reads. R1265 introduces a single backend
shared by actual IOP SCMD MMIO commands 0x40..0x43 and EE CDVD SCMD RPC
functions 0x0e..0x11. Three configuration banks implement 4, 2 and 7 physical
16-byte blocks. Read/write mode, requested count and cursor are tracked.

The provider RPC boundary uses 15 data bytes per block; physical storage
includes the modulo-256 checksum byte. Reads verify it and writes generate
it. Successful multi-block calls return the actual completed block count,
with bounded request/reply access and partial completion on errors. The
supplied XCDVDMAN confirms checksum error status is 1 and exposes the bad
block's data before returning without counting it as completed.

ABI reference: https://ps2dev.github.io/ps2sdk/scmd_8c_source.html
Detailed local protocol notes: NVRAM-CONFIG-INVESTIGATION.md.

The Wii frontend binds `sd:/pcsx2/bios-config.bin` after system initialization.
WNV1 is a 344-byte configuration-bank file: four-byte magic, 336 raw storage
bytes and little-endian CRC32. Exact size and CRC are checked before loading.
Accepted writes use a temporary file and rename; libfat's possible refusal
to overwrite an existing destination has a backup/restore fallback. A valid
backup can recover a missing primary. This is not a claim of fsync or a
fully power-loss-proof filesystem transaction. Failed saves restore the
modified in-memory block and report failure rather than success.

A bad/unreadable file is not replaced and the frontend reports saving is
disabled. In that case the emulator can still use volatile configuration.
Configuration storage survives soft IOP peripheral resets; active sessions
are cleared. Actual valid IOP reset packets clear the session as well.
Cold system initialization clears storage, then the frontend loads the file.

New optional CNFG checkpoint block uses 344 explicit bytes for session/data,
no host file path or pointer. Missing legacy blocks start with empty banks;
invalid size, cursor, reserved bytes and duplicate blocks are rejected.
Loading a checkpoint does not automatically rewrite the persistent file.
Bind a test's file before loading its checkpoint if subsequent real BIOS
writes should persist. ICDV unused struct padding is now zeroed on save.

## Validation

- Native regression suite: 174/174 pass. Added bank/persistence, genuine
  SifSetDma RPC-to-MMIO integration, and CNFG checkpoint fixtures. Coverage
  includes all banks, limits, wrong modes, hardware checksums, real provider
  error status, partial replies, corrupt files, failed-write rollback,
  backup recovery, checkpoint round trips, malformed/duplicate rejection
  and legacy defaults. See suite_results.json and native_regressions_r1265.txt.
- Both devkitPPC Wii ELF/DOL variants build. Actual emitted PPC execution
  under Unicorn passes 186 checks per ELF, including 19 config checks.
  These exercise the shared interpreter/RPC/MMIO code, not generated JIT
  blocks and not physical Wii performance.
- R1264 full coldboot and repeated Browser Back evidence remains in its
  handoff and logs. Its reboot-selected MCSERV metadata fix is retained.

## Controller-only runtime evidence

A legacy development setup checkpoint was first resumed to inspect the
configuration path. Its final wizard screen had already passed the old
placeholder write path; simply confirming that final screen cannot prove
new persistence. That preliminary run produced no file and is not used as
persistence evidence.

The actual write test instead starts at the R1264 main menu, opens System
Configuration, enters Screen Size, selects Full with RIGHT and confirms
with CROSS. All navigation uses real controller inputs. At
EE=1,737,021,793 / PC=0x002329f8 / HALT=0, the menu displays Screen Size Full.
The actual BIOS writes produce a valid 344-byte file with a valid physical
block checksum. The OSD config's initialized bit is set by those writes;
no production or test code forces that bit. The read-only
verify_config_file_r1265.py fixture verifies this specific Full-screen test.

A genuinely fresh boot loads that file after system_init. Early state at
EE=51,198,617 retains selected MCSERV/MCMAN versions 0x208/0x209, live
libmc.server 0x1000 and a real GETINFO no-card reply -11; all five existing
read-only early-boot checks pass. Real controller CROSS/release input is
used later during boot, with no guest PC/RAM edits.

The fresh boot reaches the actual main-menu display at EE=938,692,567,
PC=0x0022ddfc, HALT=0 and skips first-run setup. Subsequent controller input
reaches Browser No data at EE=1,029,546,973 / PC=0x00263eb8 / HALT=0.
CIRCLE/release returns to the menu at EE=1,075,039,513 / PC=0x0022e018,
HALT=0. See Coldboot-Menu / Browser / Back PNGs and the corresponding logs.

The same fresh-boot state then opens System Configuration and selects the
Screen Size row using DOWN/CROSS/release only. At EE=1,251,345,499,
PC=0x002320c4, HALT=0 the actual OSDSYS display reads Screen Size Full.
This confirms real BIOS settings readback across a new system initialization,
not only backend file round trips. See Coldboot-Settings.png and its log.

The final dispatch also limits RPC request data to the preceding DMA
descriptor's actual transfer size. A malformed larger RPC size cannot read
bytes outside that transferred payload; native and PPC fixtures cover this.
With the final bounds hardening, the real BIOS selects and writes 16:9
through CROSS/RIGHT/release input. At EE=1,347,701,197, PC=0x00272724,
HALT=0 the display shows Screen Size 16:9. The file-format, CRC, hardware
checksum and initialized-bit checks pass again for that actual write.
See Final-Settings.png, osd_final_write_r1265.log and
osd_final_config_file_r1265.json. This last hardening limits malformed
transfers; the completed fresh boot above used the already-correct
configuration storage before this additional transfer-size check.

## GS investigation and remaining work

The orb's texture is a valid 128x128 PSMCT32 image, and the actual orb draws
already select bilinear minification and magnification. A private diagnostic
which disables depth reads/writes changes 318 and 440 pixels in two sampled
menu frames but leaves the visible orb gaps. Disabling alpha testing changes
zero pixels in both frames. Neither diagnostic is merged or presented as a
hardware-correct fix. The previous Q-lifecycle experiment likewise did not
change these sampled menu frames. Raw firmware/VRAM/checkpoints are private.

Orb/3D rendering still has visible gaps. Complete Mechacon NVM, arbitrary
NVM-address operations, complete IOP reboot/IRX execution, memory-card
filesystems, complete EE/IOP/VU0/VU1 JIT and Tekken blockers remain open.
R1265 contains no new JIT speed claim. There is no physical Wii or Dolphin
measurement here; native runtime timings cannot predict Wii FPS.

## Reproduce / builds / packaging

Use tools/build_r1265.sh with DEVKITPRO, DEVKITPPC and the supplied toolchain.
Run tools/verify_regressions.py for native fixtures; run
verify_ppc_config_r1265.py ELF --nm powerpc-eabi-nm with Unicorn installed.
Controller drivers: verify_osdsys_setup_r1265.c (fresh boot),
verify_osdsys_config_r1265.c (explicit button/budget pairs).

The cumulative Claude patch/diff apply to the originally uploaded
pcsx2-wii-full-debug(1).zip source tree with `patch -p1`. Packaging verifies
an exact dry-run/application and byte-for-byte comparison, ZIP CRC and a
SHA-256 manifest. The source checkpoint includes both build variants,
regressions and evidence, but no BIOS, IRX payload, disc image or raw RAM
checkpoint. JIT naming denotes the existing partial backend; it does not
mean the requested complete port is finished.
