#include <stdio.h>
#include <string.h>
#include "core/ee/ee_core.c"
int main(void){bios_image_t bios={0};if(system_init(&bios,&bios))return 2;ee_state_t*s=ee_core_get_state();unsigned fails=0;
 for(unsigned bits=8;bits<=32;bits*=2)for(unsigned alias=0;alias<4;alias++)for(unsigned edge=0;edge<4;edge++){
  unsigned rd=alias==0?3:alias==1?1:alias==2?2:0;uint32_t max=bits==32?UINT32_MAX:(1u<<bits)-1;ee_reg128_t a={0},b={0},expected={0};
  for(unsigned n=0;n<128/bits;n++){uint32_t x=edge==0?max:edge==1?max-1:edge==2?max/2:0,y=edge==2?max/2:2;uint64_t sum=(uint64_t)x+y;uint32_t v=sum>max?max:sum;
   if(bits==32){set_lane_w(&a,n,x);set_lane_w(&b,n,y);set_lane_w(&expected,n,v);}else if(bits==16){set_lane_h(&a,n,x);set_lane_h(&b,n,y);set_lane_h(&expected,n,v);}else{set_lane_b(&a,n,x);set_lane_b(&b,n,y);set_lane_b(&expected,n,v);}}
  s->gpr[1]=a;s->gpr[2]=b;s->gpr[3].ud0=s->gpr[3].ud1=0;s->gpr[0].ud0=s->gpr[0].ud1=0;s->pc=0x80001000;s->next_pc=s->pc+4;s->cop0[12]=0;s->branch_pending=s->idle=s->halted=0;
  unsigned sa=bits==32?0x10:bits==16?0x14:0x18;ee_mem_write32(s,s->pc,(28u<<26)|(1u<<21)|(2u<<16)|(rd<<11)|(sa<<6)|0x28u);ee_step();
  ee_reg128_t zero={0};if(memcmp(&s->gpr[rd],rd?&expected:&zero,sizeof expected)){printf("FAIL bits=%u alias=%u edge=%u\n",bits,alias,edge);fails++;}
 }
 printf("Unsigned saturation: %u failures across 48 genuine EE instructions\n",fails);return fails?1:0;}
