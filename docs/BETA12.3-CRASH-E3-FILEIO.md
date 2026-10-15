# BETA12.3 – Crash E3 legacy FILEIO handoff

14 October 2026. Based on the locally tested BETA12.2 source; **not yet pushed to GitHub main**.

## Evidence and root cause

The owner's Codex run reported that *Crash Bandicoot: The Wrath of Cortex E3*
completed its ELF initialization but reached the game's error handler:
`unable to create font fonts\ariel32`. It subsequently reported the real
`New Game` title menu and early 3D output after fixing the legacy FILEIO RPC
version negotiation and OPEN layout. Those outcomes are **Codex's host
observations**, not reproduced as Wii/Dolphin results in this continuation.

Independent checks against the owner's locally supplied private E3 Mode-2 CD:

- Disc has `FONTS/ARIEL32.FNT;1` (2,208 bytes) and
  `FONTS/ARIEL32.BMP;1` (32,886 bytes).
- The game ELF embeds `PsIIlibkernl2110`. Its disc contains two actual
  provider ROM images: `SYS/IOPRP205.IMG;1` has `PsIIfileio  2050`, while
  `SYS/IOPRP211.IMG;1` has `PsIIfileio  2110` in their FILEIO IRX modules.
- The existing BETA12.2 RPC dispatcher responded to FILEIO fno `0xff`
  using a generic zero fallback, so legacy `_fs_version` checks could fail
  before any `cdrom0:` file lookup.
- The old Sony FILEIO OPEN layout puts the pathname at send-buffer offset
  `0`; the newer ps2sdk layout starts with a 32-bit open mode and stores the
  pathname at offset `4`. The older variant was not handled by BETA12.2.

## Applied implementation

1. `iop_fileio_protocol()` reads a **four-byte ASCII version only from an
   identity-checked real FILEIO_service module** and rejects truncated,
   conflicting, invalid, or nonnumeric provider strings. No general fake
   success response and no game-title hardcoding.
2. The current selected IOP image's FILEIO version is tracked separately
   from LOADFILE's version. It is reset when a new IOP image is requested,
   restored through the existing checkpoint EE-state buffer, and used for
   `SIF_SID_FILEIO` / fno `0xff` with receive-size bounds checks.
3. FILEIO OPEN recognizes both pathname-at-zero and mode-then-pathname
   layouts, choosing only when a real device prefix is present.
4. ISO9660 lookup tolerates ASCII case differences in each file component.
   The existing standard `;1` suffix fallback remains and no file bytes are
   synthesized.
5. BETA12.2 low-ELF EE native block safeguards and diagnostics are retained.

## Verified

- `test_fileio_protocol_beta12_3`: synthetic valid/invalid module versions,
  bounds, and identity: **PASS**.
- The same test against the owner's actual private FILEIO IRXs from both
  IOPRP images returns `2050` and `2110`: **PASS**.
- `test_ee_boot_rpc_errors`: exact four-byte reply, short receive-buffer
  guard, unknown-provider response, both OPEN layouts: **PASS**.
- `test_ee_boot_rpc_errors` with owner's private Crash CD: opens the real
  `ARIEL32.FNT` for both layouts and lowercase `cdrom0:` with no `;1`:
  **PASS**.
- Existing ISO loader/nested path, IOP CDVD, low ELF native admission,
  module metadata, LOADFILE, VU micro and VU JIT pair tests: **PASS**.
- Wii cross-build (devkitPPC r50 / GCC 16.1 / libogc 3.1.0): **PASS**.
  ELF/DOL structural check **PASS**, MEM1 free **2,701,996 bytes**.

## Limits and next steps

- The tests verify the transport and file resolution, **not** successful
  in-game font rendering, actual menu on Wii, or stable gameplay.
- The Codex continuation identified a real VU VI-register hazard: a
  conditional VU branch immediately after an integer-register write sees an
  older value. The PCSX2 documentation also describes chains of up to four
  operations and flag-read exceptions. BETA12.3 **does not** attempt a VU
  timing change yet, to avoid silently breaking existing VU0/VU1 blocks,
  savestates or graphics. A separate differential test against PCSX2 is the
  next controlled experiment.
- Checkpoint `EES1` is an internal host format; old small EE-state blocks
  may initialize the new version field to zero, requiring provider selection
  after IOP reboot. Public release compatibility is not implied.
- Tekken post-VS black screen, GT3 memory-card dialogs and playable race,
  VU graphics, ARM speedup and physical Wii FPS are not fixed by this patch.
- Keep private E3 BIN, extracted modules, BIOS, runtime logs and guest RAM
  outside GitHub/source archives. Tests accept a private path through the
  `GEKKO2_CRASH_E3_BIN` environment variable.

References: https://wiki.pcsx2.net/PCSX2_Documentation/PS2_VU_(Vector_Unit)_Documentation_Part_1
and https://github.com/ps2dev/ps2sdk (source-level FILEIO RPC layout).
