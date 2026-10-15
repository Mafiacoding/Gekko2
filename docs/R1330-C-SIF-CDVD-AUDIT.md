# R1330-C — SIF/CDVD RPC + game-I/O audit

This audit follows the green R1330-B ISO/ELF work at `575045a9` and deliberately separates real missing game-I/O behavior from already-existing hardware/HLE fallback paths.

## Existing paths that are already real enough to retain

### CDVD MMIO / N-command transport

`source/hw/iop_cdvd.c` already models the IOP CDVD register block and N-command parameter buffering. `READCD` / `READDVD` consume the requested LSN/count, read the genuinely mounted ISO and transfer payload through IOP DMA channel 3. Successful reads set DATA_READY in addition to COMMAND_COMPLETE; failed reads do not fabricate sector data.

This is not a title-specific path and must remain the common backend for game reads.

### SIF CDVD RPC dispatch

`source/core/ee/ee_core.c` already recognizes the real CDVDFSV N-command server (`0x80000595`) and forwards the real N-command parameter block to the CDVD MMIO interface rather than maintaining a second fake disc implementation.

The important timing boundary is already above the MMIO backend: the RPC request is completed through the existing pending REND machinery rather than returning synchronously to the EE caller. Therefore R1330-C must **not** add guessed per-sector or per-command delays merely because `dispatch_ncmd()` performs its backend copy immediately. A new timed CDVD event is only justified if a later real-game trace proves that guest-visible ordering requires a distinct completion boundary and the repository has a grounded event source to attach it to.

### REND / completion bridge

The current SIF path delivers a real SIF0 DMA REND packet and raises the existing SBUS/DMAC completion path. The repository also retains the R1215/R1222 HLE descriptor/semaphore completion bridge because the incomplete BIOS/SIFCMD model still does not reliably consume every REND packet through the real guest `_request_end()->iSignalSema()` chain.

For R1330-C this is classified as a **necessary compatibility fallback**, not a missing Dynarec implementation and not something to delete speculatively. It is generic (uses the live descriptor/semaphore ID) rather than game-specific.

### Generic FILEIO disc access

The SIF FILEIO service already routes `cdrom0:` / `cdrom1:` opens to `iop_cdvd_disc_find_file()` and reads file data through `iop_cdvd_disc_read_sector()`. This correctly shares the ISO parser introduced/hardened by R1330-B.

## Confirmed implementation gap

### FILEIO disc path is truncated to 63 bytes

The FILEIO `FIO_F_OPEN` handler currently copies the guest path into `char open_name[64]` and stops after 63 bytes. The same branch also uses a 64-byte temporary when adding `;1`.

That artificial limit is now inconsistent with R1330-B's nested ISO path traversal and can reject otherwise valid game paths before they ever reach `iso_find_path()`. It is a generic game-I/O compatibility bug, not a Tekken/GT3 workaround.

Planned correction:

1. accept the FILEIO protocol/path buffer's full supported path length instead of 63 bytes;
2. let `iso_find_path()` own the leaf `;1` fallback rather than duplicating a second truncated fallback in EE FILEIO;
3. add a focused regression using a nested `cdrom0:` path longer than 63 bytes;
4. run the isolated host suite and devkitPPC/Wii cross-build before promotion.

## Additional safety checks for this batch

- Reject/stop N-command sector arithmetic before `uint32_t` LSN wraparound; never wrap a request near `UINT32_MAX` back to sector zero.
- Keep disc-presence/type state explicit. Mounting an image alone must not fabricate tray/disc state.
- Do not infer CD vs DVD solely from 2048-vs-2352 storage format; a 2048-byte ISO container is not sufficient evidence of physical media type.
- Do not add title-specific command responses.
- Do not move EE/IOP control flow into GX.

## R1330-C completion gate

R1330-C is complete only after the confirmed generic gaps above have focused regression coverage, the full host regression set is green, and the Wii/devkitPPC cross-build plus ELF/DOL verification are green. Runtime claims still require a later Wii/Dolphin test.
