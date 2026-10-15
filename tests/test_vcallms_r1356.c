#include <stdio.h>
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/hw/vu.h"
int main(void){bios_image_t b={0};unsigned failures=0;
for(unsigned mode=0;mode<2;mode++)for(unsigned entry=0;entry<512;entry+=73){
 if(system_init(&b,&b))return 2;ee_state_t*s=ee_core_get_state();
 vu0_micro_write32(s,entry*8,0);vu0_micro_write32(s,entry*8+4,0x40000000u);
 vu0_micro_write32(s,(entry*8+8)&4095,0);vu0_micro_write32(s,(entry*8+12)&4095,0);
 s->cop2_ctrl[27]=entry;s->pc=0x80100000;s->next_pc=s->pc+4;
 ee_mem_write32(s,s->pc,mode?0x4a000039u:0x4a000038u|(entry<<6));
 if(ee_core_step()||s->halted||s->vu0_instructions_executed!=2||s->cop2_ctrl[26]!=((entry*8+16)&4095)||s->vu0_running)failures++;
}
printf("VCALLMS/VCALLMSR: %u failures, 16 entry/end-delay executions\n",failures);return failures?1:0;}
