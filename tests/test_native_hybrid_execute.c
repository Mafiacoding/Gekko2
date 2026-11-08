/* GPL-3.0-or-later. Synthetic real-core AOT -> interpreter -> AOT handoff. */
#include "core/ee/ee_core.h"
#include "core/recompiler/ee_aot.h"
#include "core/recompiler/ee_jit.h"
#include "core/recompiler/optimization.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#define CHECK(x) do{if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
static const uint32_t code[]={
 0x3c018000,0x34214000,0x78220000,0x70424008,0x7c280010,
 0xbc200000, /* CACHE: deliberately left to the existing interpreter. */
 0x00400029,0x00002028,0x00483021,0x03e00008,0x24070007};
typedef struct{ee_reg128_t r[32],hi,lo;uint32_t sa,pc,next;uint8_t branch;uint64_t retired;uint8_t data[32];} result;
static uint32_t random_word(uint32_t*x){*x^=*x<<13;*x^=*x>>17;*x^=*x<<5;return *x;}
static void put(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=v>>(i*8);}
int main(void){
 bios_image_t bios={0};CHECK(ee_core_init(&bios)==0);ee_state_t*s=ee_core_get_state();
 ee_state_t initial=*s;result snapshots[2];uint32_t seed=0x12345678;unsigned count=0,hits=0;
 const unsigned budgets[]={1,2,4,11};
 CHECK(ee_aot_block_count()==2);
 for(unsigned test=0;test<64;test++)for(unsigned bi=0;bi<4;bi++)for(unsigned mutate=0;mutate<2;mutate++){
  uint32_t words[4];for(unsigned i=0;i<4;i++)words[i]=random_word(&seed);
  for(unsigned mode=0;mode<2;mode++){
   *s=initial;memset(s->ram,0,65536);s->pc=0x80200000;s->next_pc=s->pc+4;s->cop0[12]=0;
   for(unsigned i=1;i<32;i++){s->gpr[i].ud0=UINT64_C(0x123456789abcdef0)^i;s->gpr[i].ud1=UINT64_C(0xfedcba9876543210)^i;}
   s->hi.ud0=5;s->hi.ud1=0x87654321;s->lo.ud0=6;s->lo.ud1=0xfedcba98;s->sa_reg=0x43;
   s->gpr[31].ud0=0x80210000;s->gpr[3].ud0=47;
   for(unsigned i=0;i<sizeof(code)/4;i++)put(s->ram+0x200000+i*4,code[i]);
   if(mutate)put(s->ram+0x200028,0x24070063);
   ee_jit_notify_physical_write(0x200000,sizeof code);
   for(unsigned i=0;i<4;i++)put(s->ram+0x4000+i*4,words[i]);
   gekko2_optimization_mask=mode?9u:0u;uint64_t before=ee_aot_retired_count();
   unsigned left=11;
   while(left){unsigned n=left<budgets[bi]?left:budgets[bi];CHECK(ee_core_step_n(n)==n);left-=n;CHECK(!s->halted&&!s->exc_raised_this_step);}
   if(mode){uint64_t native=ee_aot_retired_count()-before;CHECK(native>0&&native<11);hits++;}
   CHECK(s->pc==0x80210000&&s->gpr[7].ud0==(mutate?99:7));
   for(unsigned i=0;i<4;i++){uint32_t got;memcpy(&got,s->ram+0x4010+i*4,4);CHECK(got==words[i]+words[i]);}
   CHECK(s->hi.ud0==5&&s->hi.ud1==0x87654321&&s->lo.ud1==0xfedcba98);
   result*r=&snapshots[mode];memset(r,0,sizeof *r);memcpy(r->r,s->gpr,sizeof r->r);r->hi=s->hi;r->lo=s->lo;r->sa=s->sa_reg;r->pc=s->pc;r->next=s->next_pc;r->branch=s->branch_pending;r->retired=s->instructions_executed;memcpy(r->data,s->ram+0x4000,32);
  }
  CHECK(!memcmp(&snapshots[0],&snapshots[1],sizeof(result)));count++;
 }
 printf("PASS native hybrid: %u interpreter/AOT comparisons, %u AOT hits; 128-bit GPR/MMI/RAM, HI/LO/SA, budgets and changed delay slot\n",count,hits);
 return 0;
}
