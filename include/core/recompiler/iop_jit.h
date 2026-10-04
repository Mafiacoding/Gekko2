#ifndef PCSX2WII_IOP_JIT_H
#define PCSX2WII_IOP_JIT_H
#include "core/iop/iop_core.h"
/* PPC R3000A computation in bounded, cached instruction blocks, with
 * instruction-granular HLE, timer, source and IRQ boundaries. Ordinary
 * link calls and RFE compile; ROM device-table hooks and exception-heavy
 * instructions keep the original scalar path. Memory uses real helpers.
 * Single-op translation remains available for scalar dispatch/tests. */
/* Cached 1..8-slot blocks, explicit tick budget; unsupported/HLE-sensitive
 * slots call the original scalar switch and end the block. */
unsigned iop_jit_try_execute_block(iop_state_t *st,unsigned budget);
uint64_t iop_jit_get_block_runs(void);
uint64_t iop_jit_get_block_ticks(void);
uint32_t iop_jit_get_block_cache_size(void);
int iop_jit_try_execute_one(iop_state_t *st,uint32_t pc,uint32_t instr);
uint64_t iop_jit_get_executed_count(void);
uint64_t iop_jit_get_rejected_hit_count(void);
uint32_t iop_jit_get_cache_size(void);
void iop_jit_reset_for_test(void);
#endif
