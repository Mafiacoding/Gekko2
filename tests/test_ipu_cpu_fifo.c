#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/hw/ipu.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
 bios_image_t b={0};assert(!system_init(&b,&b));ee_state_t *s=ee_core_get_state();
 ipu_init();s->cop0[12]=0;s->halted=s->idle=0;s->pc=0x80100000u;s->next_pc=s->pc+4u;
 s->gpr[1].ud0=0xb000701fu;s->gpr[2].ud0=0x0123456789abcdefull;s->gpr[2].ud1=0xfedcba9876543210ull;
 ee_mem_write32(s,s->pc,(0x1fu<<26)|(1u<<21)|(2u<<16));
 uint8_t initial[128];memset(initial,0x55,sizeof initial);assert(ipu_input_write(initial,8)==8);
 uint64_t retired=s->instructions_executed;uint32_t pc=s->pc,next=s->next_pc;
 s->branch_pending=1;s->next_pc=0x80100080u;
 for(unsigned n=0;n<100;n++){
  assert(!ee_core_step());assert(s->pc==pc&&s->next_pc==0x80100080u&&s->branch_pending==1&&s->instructions_executed==retired);
 }
 ipu_profile_t profile;ipu_get_profile(&profile);assert(!profile.discarded_qwc&&profile.accepted_qwc==8);
 ipu_mmio_write32(0x10002000u,0x40000000u); /* FDEC creates real input space. */
 assert(!ee_core_step());assert(s->pc==0x80100080u&&s->instructions_executed==retired+1u);
 ipu_state_t *p=ipu_get_state();unsigned last=(p->in_head+p->in_count-1u)&7u;
 for(unsigned i=0;i<8;i++){assert(p->input[last][i]==(uint8_t)(s->gpr[2].ud0>>(i*8)));assert(p->input[last][i+8]==(uint8_t)(s->gpr[2].ud1>>(i*8)));}
 ipu_init();s->pc=pc;s->next_pc=next;s->branch_pending=0;s->gpr[1].ud0=0xb000700fu;
 ee_mem_write32(s,pc,(0x1eu<<26)|(1u<<21)|(2u<<16));ee_reg128_t before=s->gpr[2];retired=s->instructions_executed;
 for(unsigned n=0;n<100;n++){
  assert(!ee_core_step());assert(s->pc==pc&&s->next_pc==next&&s->instructions_executed==retired&&!memcmp(&s->gpr[2],&before,sizeof before));
 }
 uint8_t out[16];memset(out,0xab,16);assert(ipu_fifo_read128(0x10007000u,out)==-1);for(unsigned i=0;i<16;i++)assert(out[i]==0xab);
 /* Independent producer supplies a distinct qword; LQ publishes it once. */
 p->out_head=0;p->out_count=1;for(unsigned i=0;i<16;i++)p->output[0][i]=(uint8_t)i;
 assert(!ee_core_step());assert(s->pc==next&&s->instructions_executed==retired+1u&&p->out_count==0);
 assert(s->gpr[2].ud0==0x0706050403020100ull&&s->gpr[2].ud1==0x0f0e0d0c0b0a0908ull);
 /* Architectural LQ r0 performs no FIFO access and must not stall. */
 s->pc=pc;s->next_pc=next;ee_mem_write32(s,pc,(0x1eu<<26)|(1u<<21));retired=s->instructions_executed;
 assert(!ee_core_step()&&s->pc==next&&s->instructions_executed==retired+1u);
 puts("PASS EE/IPU FIFO: 200 stalled LQ/SQ retries, exact PC/delay/128-bit state, real input-space resume, one output consumption and LQ r0");
}
