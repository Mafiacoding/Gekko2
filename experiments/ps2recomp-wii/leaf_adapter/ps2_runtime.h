/* GPL-3.0-or-later. Test-only scalar adapter for PS2Recomp generated leaves.
 * It intentionally does not implement the PS2Recomp runtime or any devices.
 * Generated calls/intrinsics outside this subset must fail to compile. */
#pragma once
#ifndef GEKKO2_PS2RECOMP_LEAF_PROBE
#error "This header is only for isolated leaf probes, never a game runtime"
#endif
#include <cstdint>
struct alignas(16) LeafGpr { uint64_t lo,hi; };
struct alignas(16) R5900Context {
 LeafGpr r[32];
 uint32_t pc;
 bool in_delay_slot;
 uint32_t branch_pc;
 uint64_t hi,lo;
 uint32_t sa;
};
class PS2Runtime {}; // No successful device/syscall/dispatch stubs.
