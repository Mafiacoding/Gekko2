#include <stdio.h>
#include "core/ee/ee_core.c"
int main(void){bios_image_t b={0};if(system_init(&b,&b))return 2;ee_state_t*s=ee_core_get_state();dma_state_t*d=dma_get_state();unsigned fail=0;
for(unsigned kind=0;kind<4;kind++)for(unsigned done=0;done<2;done++){
 s->pc=0xa0100000;s->next_pc=s->pc+4;s->branch_pending=0;s->halted=0;s->cop0[12]=0;s->cop0[9]=1;
 d->d_pcr=0x204;d->d_stat=done?0x204:0x200;
 ee_mem_write32(s,s->pc,0x41000000u|(kind<<16)|3u);ee_mem_write32(s,s->pc+4,0x2402002au);s->gpr[2].ud0=0;
 ee_core_step();int taken=done==(kind&1u);unsigned expect=taken?0xa0100004u:((kind&2u)?0xa0100008u:0xa0100004u);
 if(s->halted||s->pc!=expect){puts("FAIL: BC0 branch/annul decision");fail++;}
 if(taken||!(kind&2u)){ee_core_step();if(s->gpr[2].ud0!=42||s->pc!=(taken?0xa0100010u:0xa0100008u)){puts("FAIL: BC0 delay slot or target");fail++;}}
}
printf("BC0 DMA: %u failures, all four variants with complete/incomplete selected channels\n",fail);return fail?1:0;}
