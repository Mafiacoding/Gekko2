#ifndef GEKKO2_IOP_LIGHTREC_H
#define GEKKO2_IOP_LIGHTREC_H
#include "core/iop/iop_core.h"
/* Pilot: register-only MULT/MULTU/DIV/DIVU bodies. Original core retains
 * instruction retirement, interrupts, load delays, PC and HLE routing. */
/* Reserve the bounded pool before ordinary blocks fragment the arena. */
int iop_lightrec_prepare(void);
int iop_lightrec_try_execute(iop_state_t *state,uint32_t word);
void iop_lightrec_reset(void);
uint64_t iop_lightrec_stat(unsigned index); /* ready, calls, misses, failures, code bytes, eligible attempts, fallbacks */
#endif
