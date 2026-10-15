#include <assert.h>
#include <stdio.h>
#include "core/hw/ipu.h"
#include "core/hw/ee_intc.h"
#include "core/hw/dma.h"
int main(void){
 for(unsigned idp=0;idp<3;idp++)for(unsigned next=0;next<3;next++){
  dma_init();ipu_init();ee_intc_init();ipu_state_t*s=ipu_get_state();
  ipu_mmio_write32(0x10002010,idp<<16);
  ipu_mmio_write32(0x10002000,0x28000000u);
  assert(s->mpeg.dc_precision==idp);
  for(unsigned c=0;c<3;c++)s->mpeg.dc[c]=900+c;
  s->mpeg.phase=1;s->mpeg.index=17;s->busy=1;s->th0=77;s->th1=99;
  ipu_mmio_write32(0x10002010,0x40000000u|(next<<16));
  assert(!s->busy&&!s->in_count&&!s->out_count&&!s->th0&&!s->th1);
  for(unsigned c=0;c<3;c++)assert(s->mpeg.dc[c]==(idp?900+(int)c:128));
  assert(ee_intc_get_raise_count(8)==1);
  /* BDEC without DCR must inherit reset predictions, not old stream values. */
  ipu_mmio_write32(0x10002000,0x28000000u);
  assert(s->mpeg.dc_precision==next);
  for(unsigned c=0;c<3;c++)assert(s->mpeg.dc[c]==(idp?900+(int)c:128));
 }
 puts("PASS IPU soft-reset DC prediction, latched precision across CTRL changes, BDEC continuation and IRQ");return 0;
}
