/* Guest EE accesses, not direct host pokes: synthetic SIGNAL handshake. */
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/hw/gs.h"
#include "core/hw/gif.h"
#include "core/hw/ee_intc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void put(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static void reset(void){gs_init();gif_init();ee_intc_init();}
int main(void)
{
 bios_image_t b={0};assert(!system_init(&b,&b));ee_state_t *s=ee_core_get_state();
 uint8_t p[48]={0};put(p,0x8002);put(p+4,1u<<28);put(p+8,14);
 put(p+16,0x1234);put(p+20,0xffff);put(p+24,GS_REG_SIGNAL);
 put(p+32,0xabcd0000);put(p+36,0xffff0000);put(p+40,GS_REG_SIGNAL);
 for(unsigned alias=0;alias<3;alias++){
  uint32_t prefix=alias==0?0:alias==1?0x80000000u:0xa0000000u;
  for(unsigned width=1;width<=8;width*=2){
   reset();assert(gif_try_process_quadwords(GIF_PATH_1,p,3)==2);
   assert(ee_mem_read32(s,prefix|0x12001000u)&1u);
   if(width==1)ee_mem_write8(s,prefix|0x12001000u,1);
   if(width==2)ee_mem_write16(s,prefix|0x12001000u,1);
   if(width==4)ee_mem_write32(s,prefix|0x12001000u,1);
   if(width==8)ee_mem_write64(s,prefix|0x12001000u,1);
   assert(!(gs_get_state()->csr&1));
   assert(gif_try_process_quadwords(GIF_PATH_1,p+32,1)==1);
   assert(gs_get_state()->siglblid==0xabcd1234u);
   assert(ee_mem_read32(s,prefix|0x12001080u)==0xabcd1234u);
  }
 }
 /* Masked event then IMR byte1 unmask: edge once, no invented CSR ACK. */
 reset();assert(gs_raise_event(0)==0);assert(!ee_intc_get_raise_count(0));
 ee_mem_write8(s,0x12001011u,0x1e);
 assert(ee_intc_get_raise_count(0)==1&&(gs_get_state()->csr&1));
 ee_mem_write8(s,0x12001011u,0x1e);assert(ee_intc_get_raise_count(0)==1);
 ee_mem_write8(s,0x12001011u,0x1f);assert(ee_intc_get_raise_count(0)==1);
 ee_mem_write8(s,0x12001011u,0x1e);assert(ee_intc_get_raise_count(0)==2);
 /* Independent bit-by-bit pending-event/unmask matrix (five CSR events). */
 for(unsigned events=0;events<32;events++)for(unsigned old=0;old<32;old++)for(unsigned next=0;next<32;next++){
  reset();gs_get_state()->csr=events;gs_get_state()->imr=old<<8;
  unsigned expected=0;for(unsigned bit=0;bit<5;bit++)
   if((events&(1u<<bit))&&(old&(1u<<bit))&&!(next&(1u<<bit)))expected=1;
  ee_mem_write32(s,0xb2001010u,next<<8);
  assert(ee_intc_get_raise_count(0)==expected);
  assert(gs_get_state()->csr==events&&gs_get_state()->imr==((next<<8)|0x6000u));
 }
 /* Real SB/SH/SW/SD instructions must reach the same CSR action path. */
 const unsigned stores[]={40,41,43,63};
 for(unsigned i=0;i<4;i++){
  reset();s->pc=0x80020000u;s->next_pc=s->pc+4;s->cop0[12]=0;s->branch_pending=0;s->halted=0;
  s->gpr[1].ud0=0xb2001000u;s->gpr[2].ud0=1;gs_get_state()->csr=3;
  put(s->ram+0x20000,(stores[i]<<26)|(1u<<21)|(2u<<16));
  assert(ee_core_step_n(1)==1&&!s->exc_raised_this_step&&gs_get_state()->csr==2);
 }
 /* CSR lane writes clear only the written events, never merge read bits. */
 gs_get_state()->csr=0x201full;
 ee_mem_write8(s,0x12001001u,0);assert(gs_get_state()->csr==0x201full);
 ee_mem_write16(s,0x12001000u,1);assert(gs_get_state()->csr==0x201eull);
 ee_mem_write32(s,0x12001004u,0xffffffff);assert(gs_get_state()->csr==0x201eull);
 /* Ordinary privileged setup registers keep unwritten byte lanes. */
 ee_mem_write64(s,0x12000070u,UINT64_C(0x123456789abcdef0));
 ee_mem_write16(s,0x12000072u,0x4321);
 assert(gs_get_state()->dispfb1==UINT64_C(0x123456784321def0));
 assert(ee_mem_read16(s,0x12000076u)==0x1234);
 puts("PASS EE GS byte/half/word/dword aliases, SIGNAL replay, pending-event IMR unmask and W1C lanes");return 0;
}
