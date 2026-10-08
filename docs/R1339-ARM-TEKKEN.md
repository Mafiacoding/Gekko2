# R1339: bounded ARM loading and Tekken investigation

The supplied physical-Wii log is named R1337 but identifies BUILD R1338.
FIRST_IMAGE is 509303 ms versus 552449 ms in the prior beta log: 7.81% less
elapsed time. Exclusive translation samples before FIRST_IMAGE account for
about 4.3% versus 10.8%. These are logged observations, not a new FPS forecast.
The user reports 0.11--0.13 FPS. Scheduler and EE execution remain substantial.

IOS222 reload succeeded (original IOS58, active IOS222). ARM_LOADER=-6,
available=0, submitted=0, completed=0. ARM did not produce that speed increase.
BIOS issued no CSC commands, so the existing CSC worker cannot accelerate this
BIOS phase even when successfully loaded. EE/IOP and cache remain on PPC.

## ARM fix

The previous worker default 0x137f0000 lies outside the checked upstream
MLOAD implementation's executable allocation 0x13700000, size 0x80000.
Use 0x13700000 for the new worker. Continue requiring the actual installed
IOS to authorize every segment through GET_LOAD_BASE. No limits are widened,
no occupied memory is overwritten, and no IOS/NAND installer is included.
The supplied console log did not record its actual allocation; the exact
cause of its -6 rejection is therefore still to be confirmed on hardware.

ARM_LOAD_DETAIL now records file bytes, IOS base/capacity, entry, file CRC32
and validation stage: 1 header, 2 segments, 3 entry, 4 IOS metadata, 5 stack.
Replace BOTH boot.dol and sd:/pcsx2/arm/Gekko2-ARM-Worker.elf. Enable ARM and
IOS222, save and fully restart the application. A cold boot alone does not
reload IOS. available=1 verifies the worker capability handshake; actual CSC
submitted/completed counters are required to demonstrate work.

## Tekken Tag Tournament SLUS-20001

The user-provided split image was privately extracted and booted through the
normal cold disc launch. Resource reads and GIF submissions occur. A native
interpreter run reaches the FMV preparation wait, but the captured frame is
black and there is no MPEG command yet. Native timing is not Wii/PPC timing.

The apparent VBlank loop is working: each real VBlank exits the poll and the
guest acknowledges INTC bit 4. Its caller instead repeatedly queries the
retail RSPU2DRV extension command 0x2030 and waits for bit 0x40, with a 900-frame
timeout. Current generic SPU2 HLE returns zero. The privately verified module
(size 66077, FNV1a e16bba10) computes that status from real worker state;
bit 0x40 denotes state 1 at its stream-state halfword. The module initializes
that state to 1 and changes it for stream requests. A correct implementation
must model initialization, stream operations and completion, not return a
constant success bit. R1339 does not introduce such an unverified shortcut.
Existing resource read HLE (0x204e/0x2045) remains scoped to this exact module.

Remaining work: implement the retail streaming service state machine and
actual bounded disc streaming, preserve its state in checkpoints, then test
natural decoder commands and framebuffer output. Title/FMV is NOT verified.
No copyrighted ROM, disc, module or private runtime dump is included.

Upstream references inspected: wiidev/usbloadergx source/mload/mload.c;
xerpi/mload-mod link.ld and source/main.c (GET_LOAD_BASE). Existing GPL credits
remain intact. Hardware ARM loading and acceleration remain unverified.
