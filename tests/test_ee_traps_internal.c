/* Architectural trap delivery, including zero/delay/nested exception cases. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/ee/ee_core.h"
static void write32(uint8_t *p,uint32_t v){for(unsigned k=0;k<4;k++)p[k]=(uint8_t)(v>>(k*8));}
int main(void){
 bios_image_t bios={0};bios.data=calloc(1,BIOS_MAX_SIZE);bios.size=BIOS_MAX_SIZE;bios.loaded=1;
 if(!bios.data||ee_core_init(&bios))return 2;
 ee_state_t *s=ee_core_get_state();uint8_t *ram=s->ram;unsigned ram_size=s->ram_size;
 const uint64_t values[]={0,1,0x7fffffff,0x80000000,UINT64_MAX,0x7fffffffffffffffULL,0x8000000000000000ULL,0xffffffff00000000ULL};
 const unsigned kinds[]={48,49,50,51,52,54};unsigned cases=0;
 for(unsigned immediate=0;immediate<2;immediate++)for(unsigned f=0;f<6;f++)
 for(unsigned delay=0;delay<2;delay++)for(unsigned nested=0;nested<2;nested++)
 for(unsigned bev=0;bev<2;bev++)for(unsigned a=0;a<8;a++)for(unsigned b=0;b<8;b++){
  memset(s,0,sizeof(*s));s->ram=ram;s->ram_size=ram_size;s->pc=0x80001000;s->next_pc=s->pc+4;
  s->cop0[9]=100;s->cop0[11]=UINT32_MAX;s->cop0[12]=(nested?2:0)|(bev?0x400000:0);s->cop0[14]=0x12345678;
  s->gpr[1].ud0=values[a];s->gpr[1].ud1=0x123456789abcdef0ULL;s->gpr[2].ud0=values[b];s->gpr[2].ud1=0xfedcba9876543210ULL;
  ee_reg128_t before[32];memcpy(before,s->gpr,sizeof(before));uint64_t rhs=immediate?(uint64_t)(int64_t)(int16_t)values[b]:values[b];
  unsigned kind=kinds[f],rt=kind-40;uint32_t instruction=immediate?((1u<<26)|(1u<<21)|(rt<<16)|(uint16_t)values[b]):((1u<<21)|(2u<<16)|kind);
  int take=kind==48?(int64_t)values[a]>=(int64_t)rhs:kind==49?values[a]>=rhs:kind==50?(int64_t)values[a]<(int64_t)rhs:kind==51?values[a]<rhs:kind==52?values[a]==rhs:values[a]!=rhs;
  if(delay){write32(ram+0x1000,(4u<<26)|2u);write32(ram+0x1004,instruction);ee_core_step();}else write32(ram+0x1000,instruction);
  ee_core_step();
  if(memcmp(before,s->gpr,sizeof(before)))return 3;
  if(take){if(s->pc!=(bev?0xbfc00380u:0x80000180u)||(s->cop0[13]&0x8000007cu)!=(0x34u|(delay&&!nested?0x80000000u:0))||s->cop0[14]!=(nested?0x12345678u:0x80001000u))return 4;}
  else if(s->pc!=(delay?0x8000100cu:0x80001004u)||(s->cop0[13]&0x7cu))return 5;
  cases++;
 }
 /* A synchronous trap wins over a timer which becomes serviceable here. */
 memset(s,0,sizeof(*s));s->ram=ram;s->ram_size=ram_size;s->pc=0x80001000;s->next_pc=s->pc+4;
 s->cop0[9]=100;s->cop0[11]=99;s->cop0[12]=0x18001;write32(ram+0x1000,0x34);ee_core_step();
 if(s->pc!=0x80000180u||(s->cop0[13]&0x7cu)!=0x34u)return 6;
 ee_core_shutdown();free(bios.data);printf("PASS %u EE traps and synchronous/timer priority\n",cases);return 0;
}
