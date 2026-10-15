#ifndef GEKKO2_IOP_BLOCK_BOUNDARIES_H
#define GEKKO2_IOP_BLOCK_BOUNDARIES_H
#include <stdint.h>
/* R3000A flow changes, exception instructions and COP0 control writes.
 * Conservative barrier for JIT block formation, not a new opcode decoder. */
static inline int iop_block_requires_boundary(uint32_t w)
{
    unsigned op=w>>26,fn=w&63u;
    return (op>=1u&&op<=7u) || op==0x10u ||
           (op==0u&&(fn==8u||fn==9u||fn==12u||fn==13u));
}
#endif
