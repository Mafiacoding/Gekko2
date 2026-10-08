#include "core/hw/frontend_runtime.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 frontend_pad_latch p={0};
 assert(frontend_pad_latch_update(&p,8,0)==8);
 assert(frontend_pad_latch_update(&p,0,0)==8);
 assert(frontend_pad_latch_update(&p,0,1)==0);
 assert(frontend_pad_latch_update(&p,8,1)==8);
 assert(frontend_pad_latch_update(&p,8,2)==8);
 assert(frontend_pad_latch_update(&p,0,2)==0);
 assert(frontend_pad_latch_update(&p,1,2)==1);
 assert(frontend_pad_latch_update(&p,2,2)==3);
 assert(frontend_pad_latch_update(&p,0,3)==0);
 p.samples=UINT32_MAX;
 assert(frontend_pad_latch_update(&p,0,0)==0);
 puts("PASS input: short presses, sample acknowledgement, held/released, combined and counter wrap");
}
