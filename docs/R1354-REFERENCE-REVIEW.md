# Source review and transfer decisions,2026-10-09

Reviewed primary code, not inferred solely from release notes. No external
engine code was copied into the new IOP/HLE implementation.

## Play!, including mobile

Reviewed commit83700b2c31e593bc94e845b4b31b797be84dda59:
- https://github.com/jpd002/Play-/blob/83700b2c31e593bc94e845b4b31b797be84dda59/Source/iop/IopBios.cpp
- https://github.com/jpd002/Play-/blob/83700b2c31e593bc94e845b4b31b797be84dda59/Source/iop/Iop_Sysclib.cpp
- https://github.com/jpd002/Play-/blob/83700b2c31e593bc94e845b4b31b797be84dda59/Source/iop/Iop_Thbase.cpp
- https://github.com/jpd002/Play-/blob/83700b2c31e593bc94e845b4b31b797be84dda59/Source/ui_android/CMakeLists.txt

IopBios registers named IOMAN,SYSMEM,LOADCORE,THREADMAN,SIF,CDVD,etc. modules;
exception dispatch invokes a module by function ID. Reschedule saves/loads
ready thread contexts or selects its idle function. Android links the same
PlayCore and chooses OpenGL/Vulkan presentation. It does not contain a special
Android HLE trick that makes a Wii IOS worker execute the MIPS CPUs.
Sysclib identifies12 memcpy,13 memmove,14 memset and returns the destination
pointer. Transferred idea: typed registered native services, not blanket
replacement of the running IOP or hardcoded BIOS-PC guesses.

Play!'s own README documents an internal HLE BIOS with no external BIOS support:
https://github.com/jpd002/Play-
That is a broad architecture, not a switch we can safely turn on in Gekko2.
Its compatibility tracker does not establish full speed for every game/device.
No claimed Wii speed ceiling or promise is derived from its mobile performance.

## PCSX2

Reviewed current master files:
https://github.com/PCSX2/pcsx2/blob/master/pcsx2/IopBios.cpp
https://github.com/PCSX2/pcsx2/blob/master/pcsx2/R5900.cpp

IopBios irxImportHLE selects library-name/ordinal replacements, including
loadcore registration and host filesystem services; unhandled imports return
to the original route. R5900 cpuEventTest accounts EE/IOP cycle debt, invokes
IOP ExecuteBlock when needed, handles events and schedules the next deadline.
Transferred idea: bounded IOP catch-up using native blocks and explicit service
allow-lists. Our scheduler retains its documented instruction-slot8:1 model;
it is not PCSX2's cycle model. Complete THREADMAN/CDVD replacement requires
correct shared service/thread semantics and is not claimed implemented here.

PS2SDK is the ABI authority for the new sysclib ordinals:
https://github.com/ps2dev/ps2sdk/blob/master/iop/system/sysclib/include/sysclib.h
https://github.com/ps2dev/ps2sdk/blob/master/iop/system/sysclib/src/exports.tab

## nullDC4Wii

Reviewed current README option descriptions:
https://github.com/BenoitAdam/nullDC4Wii/blob/main/README.md
R1351's actual DSI implementation already cites the inspected wii_fastmem.cpp
commit26a623e3feca9927d8ffe43706b06d82d672dd64.

Options distinguish32-byte JIT alignment, opcode-specific fallback spills,
pinned FPU registers, dynamic target caches, underclock and diagnostics.
Gekko2 already allocates its code arena/entries at32-byte alignment and has
guarded residency, cache reuse and a real DSI backpatch pilot. Adding duplicate
menu switches would not speed them up. The new IOP adaptive owner option tackles
our observed short-block cache limitation. It is our implementation, not a port
of SH4 code or proof that nullDC's gain applies to MIPS.
Cross-block FPU pinning needs PS2 FPU/VU semantics and helper/ABI proofs; SH4
register pinning cannot be copied blindly. Underclock changes guest timing and
is not smuggled into the cache/scheduler fixes. No fabricated frames.

## Limits and next measurements

Wii logs must demonstrate actual registered HLE calls, IOP block growth, fewer
shared barriers and comparable guest_vblank/EE_per_s or first-image latency.
ARM RAM assistance is synchronous and verified; it can cost more than CPU copy.
Existing zero-vertex geometry and source/mapping cache pressure still need
independent fixes. Public mobile code is a design reference; unverified private
mobile-emulator internals and universal full-speed claims are not evidence.
