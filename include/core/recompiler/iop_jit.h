#ifndef PCSX2WII_IOP_JIT_H
#define PCSX2WII_IOP_JIT_H
#include "core/iop/iop_core.h"
/* Direct PPC R3000A register computation; one instruction per dispatch.
 * HLE-sensitive calls, link branches, exceptions and unsupported
 * operations interpret. Integer memory uses real IOP helpers; ordinary
 * branches/jumps and safe divide compile.
 * iop_core keeps its original HLE prelude and hardware interrupt epilogue. */
int iop_jit_try_execute_one(iop_state_t *st,uint32_t pc,uint32_t instr);
uint64_t iop_jit_get_executed_count(void);
uint64_t iop_jit_get_rejected_hit_count(void);
uint32_t iop_jit_get_cache_size(void);
void iop_jit_reset_for_test(void);
#endif
