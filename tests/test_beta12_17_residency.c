#include <stdint.h>
#include <stdio.h>
#include "core/recompiler/ppc_dynarec.c"
int main(void) {
    ppc_residency a={0};
    /* MMI2 PEXEW reads four 32-bit lanes from r5, writes all four to r6.
       Use a repeated nonbranching emitter, so the planner can keep hot lanes. */
    uint32_t iw=(0x1cu<<26)|(4u<<21)|(5u<<16)|(6u<<11)|(30u<<6)|9u;
    uint32_t words[]={iw,iw,iw};
    residency_plan(&a,words,3,1,0);
    unsigned count=0;
    for(unsigned lane=0;lane<4;lane++)count+=(a.slot[5u*4u+lane]>=0);
    printf("BETA12.17 EE resident 128-bit lanes: slots=%u r5=%u\n",a.count,count);
    return count!=4u;
}
