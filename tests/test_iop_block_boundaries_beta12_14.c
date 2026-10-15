#include <assert.h>
#include <stdio.h>
#include "core/recompiler/iop_block_boundaries.h"
int main(void){
 assert(iop_block_requires_boundary(0x0000000cu)); /* SYSCALL */
 assert(iop_block_requires_boundary(0x0000000du)); /* BREAK */
 assert(iop_block_requires_boundary(0x00000008u)); /* JR */
 assert(iop_block_requires_boundary(0x00000009u)); /* JALR */
 assert(iop_block_requires_boundary(0x1000ffffu)); /* BEQ */
 assert(iop_block_requires_boundary(0x42000010u)); /* RFE */
 assert(iop_block_requires_boundary(0x40886000u)); /* MTC0 Status */
 assert(!iop_block_requires_boundary(0x24020001u)); /* ADDIU */
 assert(!iop_block_requires_boundary(0x8c830000u)); /* LW */
 puts("PASS IOP native block exception/control boundaries");return 0;
}
