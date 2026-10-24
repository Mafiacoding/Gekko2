/* GPL-3.0-or-later. Entry used by host/PPC semantic probes, not a game boot. */
#include <cstddef>
#include "ps2_runtime.h"
#include "ps2_recompiled_functions.h"
#ifndef RECOMP_LEAF_FUNCTION
#define RECOMP_LEAF_FUNCTION probe_0x200000
#endif
extern "C" void recomp_leaf(R5900Context *ctx) {
 RECOMP_LEAF_FUNCTION(nullptr,ctx,nullptr);
}
extern "C" const unsigned recomp_leaf_layout[] __attribute__((section(".leaf_layout"),used))={
 sizeof(R5900Context),offsetof(R5900Context,r),offsetof(R5900Context,pc),
 offsetof(R5900Context,in_delay_slot),offsetof(R5900Context,branch_pc),
 offsetof(R5900Context,hi),offsetof(R5900Context,lo),offsetof(R5900Context,sa)
};
