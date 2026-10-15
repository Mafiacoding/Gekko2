# Mobile PS2 emulation and Gekko2's HLE boundary

Source inspection 2026-10-08: Play! commit
83700b2c31e593bc94e845b4b31b797be84dda59. This is a concrete open-source
Android/iOS comparison, not a description of every mobile PS2 emulator.
No code is copied.

Play! executes guest CPU basic blocks through its MIPS executor/JIT and
contains separate EE, IOP, VU, IPU and GS components. The target generator
produces host code rather than executing MIPS instructions directly on ARM.
GenericMipsExecutor.h implements compiled block lookup, linking and cache
invalidation. EeExecutor.cpp contains platform memory/protection integration
and block compile hints. Its supported ARM/ARM64 targets do not imply that
Gekko2's PPC machine code can run on the Wii ARM without another backend.

The public README explicitly describes a built-in high-level BIOS, and
IopBios.cpp registers replacements for modules such as sysmem, modload,
thread/semaphore services, CDVD, file I/O and SIF. It also distinguishes HLE
module loading from actual guest modules. Consequently HLE is compatible
with a real PS2 emulator design; the difficulty is preserving the observable
service contract and supporting guest/custom modules that are not replaced.

For Gekko2, retain verified replacements and execute unsupported guest code
when a valid implementation can run on the emulated IOP. An unconditional
success return without the required output, callbacks, memory effects,
thread/semaphore behavior or IRQ completion is not a compatibility fix.
Diagnose the first incorrect service boundary before optimizing it. Replacing
all HLE with low-level BIOS/module execution would also increase CPU work;
adding more HLE without a tested contract risks hiding the original failure.

For the owner's Tekken request: a new real BIOS/disc boot is required to prove
progress beyond Namco. Old screenshots and a non-halted log do not prove a
current title screen. Native host execution tests functional behavior, while
linked PPC tests and physical Wii runs are separate evidence for JIT/GX.

References inspected:
- https://github.com/jpd002/Play-/blob/83700b2c31e593bc94e845b4b31b797be84dda59/README.md
- https://github.com/jpd002/Play-/blob/83700b2c31e593bc94e845b4b31b797be84dda59/Source/GenericMipsExecutor.h
- https://github.com/jpd002/Play-/blob/83700b2c31e593bc94e845b4b31b797be84dda59/Source/ee/EeExecutor.cpp
- https://github.com/jpd002/Play-/blob/83700b2c31e593bc94e845b4b31b797be84dda59/Source/iop/IopBios.cpp
- https://www.purei.org/about.php
