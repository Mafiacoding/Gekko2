#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "core/ee/ee_core.h"
#include "core/hw/vu.h"
int main(void) {
 static ee_state_t st;memset(&st,0,sizeof st);vu1_init();
 const uint32_t bases[]={0x11000000,0x11004000,0x11008000,0x1100c000};
 for(unsigned b=0;b<4;b++) {
  uint8_t *target=b==0?st.vu0_micro:b==1?st.vu0_mem:b==2?vu1_get_state()->micro:vu1_get_state()->mem;
  for(unsigned alias=0;alias<3;alias++) {
   uint32_t a=bases[b]|(alias==1?0x80000000:alias==2?0xa0000000:0);
   uint32_t value=0x12345678+b*16+alias;
   ee_mem_write32(&st,a+32,value);
   assert(target[32]==(uint8_t)value&&target[35]==0x12);
   assert(ee_mem_read32(&st,a+32)==value);
   ee_mem_write8(&st,a+33,0x5a);assert(target[33]==0x5a);
   ee_mem_write16(&st,a+34,0xcdef);assert(target[34]==0xef&&target[35]==0xcd);
   ee_reg128_t q={0},out={0};q.ud0=0x0123456789abcdefULL;q.ud1=0xfedcba9876543210ULL;
   ee_mem_write128(&st,a+64,&q);ee_mem_read128(&st,a+64,&out);
   assert(q.ud0==out.ud0&&q.ud1==out.ud1);
   assert(!st.mem_tlb_miss);
  }
  if(b<2)for(unsigned mirror=1;mirror<4;mirror++)assert(ee_mem_read32(&st,bases[b]+mirror*4096+32)==ee_mem_read32(&st,bases[b]+32));
 }
 /* A mapped alias retains V/D checks before accessing VU storage. */
 st.cop0[10]=7;st.tlb[0].entry_hi=0x00300007;
 st.tlb[0].entry_lo0=(0x1100cu<<6)|6;st.tlb[0].entry_lo1=(0x1100du<<6)|6;
 ee_mem_write32(&st,0x00300020,0xaabbccdd);assert(ee_mem_read32(&st,0xb100c020)==0xaabbccdd);
 puts("PASS EE VU0/VU1 micro/data: byte/half/word/quad, KSEG0/1, VU0 mirrors and translated aliases");return 0;
}
