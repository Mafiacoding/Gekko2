# BETA12.31d Wii IOS UDP compatibility fix

Real-Wii feedback reached DHCP but failed during socket creation with libogc
error -123 (EPROTONOSUPPORT). The sender passed POSIX IPPROTO_UDP=17. Wii IOS
expects protocol 0 for this socket interface; SOCK_DGRAM selects UDP. Correct
that argument. This is a sender-side SDK contract error, before packet delivery.

Also correct the nonblocking flag: net_fcntl forwards flags directly to IOS,
where bit 4 selects nonblocking IO. The current newlib O_NONBLOCK is 0x4000
(and network.h has a separate legacy definition); neither is the IOS bit.
Use the IOS value 4 while preserving the checked F_SETFL result and failure
stage. Without this correction, receive could remain blocking on the worker.
No protocol/buffer/CPU/GX/experiment preference change. Windows receiver 1.1
and the existing saved endpoint remain compatible. Factory network default OFF.

## Evidence and validation

The delivered BETA12.31c libogc disassembly shows net_socket forwarding all three
arguments in IOS SO_SOCKET ioctl 15, and net_fcntl forwarding flags in ioctl 5.
The devkitPPC errno header maps EPROTONOSUPPORT to 123. Dolphin's Wii IOS network
model independently rejects nonzero socket protocols and uses nonblock bit 4.
No upstream implementation copied; see references pinned in the validation record.

The strengthened PPC worker test rejects the old ELF with observed socket args
(2,2,17), instead of blindly returning a simulated socket. Current worker checks
require socket args (2,2,0) and F_SETFL flags (4,4). Four additional cases execute
the actual linked libogc wrappers/error conversion up to modeled IOS IPC:
protocol 17 maps to -123, protocol 0 creates a socket, 0x4000 leaves IOS blocking,
and bit 4 enables nonblocking. SDK-boundary behavior is now explicitly covered.

Re-run host collector/menu/config/profile-work tests, linked PPC wire/FIFO/ABI,
worker and IOS-boundary tests and fresh devkitPPC DOL/ELF/MEM1 gates. Physical
Wii receipt of an ACK still needs the short menu test. No BIOS/game/GX/FPS fix.
