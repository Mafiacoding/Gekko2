#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/hw/vif.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static void put(uint8_t *p,uint32_t w){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(w>>(i*8));}
int main(void)
{
 bios_image_t b={0};assert(!system_init(&b,&b));ee_state_t *s=ee_core_get_state();
 for(unsigned channel=0;channel<2;channel++){
  vif_init();s->cop0[12]=0;s->halted=s->idle=0;s->branch_pending=0;
  vif_state_t *v=channel?vif1_get_state():vif0_get_state();
  uint8_t code[4];put(code,0x87000001u); /* Actual I-bit MARK stalls VIF. */
  if(channel)vif1_process_tag_words(1,code,1);else vif0_process_tag_words(0,code,1);
  assert(v->irq_stalled&&!v->stall_words);
  s->pc=0x80100000;s->next_pc=s->pc+4;s->gpr[1].ud0=channel?0xb0005000:0xb0004000;
  s->gpr[2].ud0=UINT64_C(0x0700123507001234);s->gpr[2].ud1=UINT64_C(0x0700123707001236);
  unsigned capacity=channel?16u:8u;
  for(unsigned i=0;i<=capacity;i++)ee_mem_write32(s,s->pc+i*4,(0x1fu<<26)|(1u<<21)|(2u<<16));
  for(unsigned i=0;i<capacity;i++)assert(!ee_core_step());
  assert(v->stall_words==capacity*4u&&v->irq_stalled);
  uint32_t stat=0;assert(vif_mmio_read32(channel?0x10003c00u:0x10003800u,&stat));assert((stat>>24)==capacity);
  uint8_t before[256];memcpy(before,v->stall_buffer,capacity*16u);
  uint64_t retired=s->instructions_executed;uint32_t pc=s->pc;
  s->branch_pending=1;s->next_pc=0x80100100;
  s->gpr[2].ud0=UINT64_C(0x0700567907005678);s->gpr[2].ud1=UINT64_C(0x0700567b0700567a);
  for(unsigned i=0;i<100;i++){
   assert(!ee_core_step());assert(s->pc==pc&&s->next_pc==0x80100100&&s->branch_pending);
   assert(s->instructions_executed==retired&&v->stall_words==capacity*4u);
   assert(!memcmp(before,v->stall_buffer,capacity*16u));
  }
  assert(vif_mmio_write32(channel?0x10003c10:0x10003810,8));
  assert(!v->irq_stalled&&!v->stall_words&&v->mark==0x1237);
  assert(!ee_core_step());assert(s->pc==0x80100100&&s->instructions_executed==retired+1);
  assert(v->mark==0x567b&&!v->stall_words);
 }
 puts("PASS VIF0/1 guest SQ bounded FIFO, 200 full retries, delay/PC/retirement preservation and exactly-once STC resume");return 0;
}
