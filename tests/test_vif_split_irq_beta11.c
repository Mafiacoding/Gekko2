#include <assert.h>
#include <stdio.h>
#include "core/hw/vif.h"
#include "core/hw/vu.h"
#include "core/hw/gs.h"
#include "core/hw/gif.h"
#include "core/hw/ee_intc.h"
static void put(unsigned char *b,unsigned n,unsigned v){for(unsigned k=0;k<4;k++)b[n*4+k]=v>>(k*8);}
int main(void){
 unsigned char a[16]={0},b[16]={0};
 for(unsigned kind=0;kind<2;kind++){
  ee_intc_init();gs_init();gif_init();vu1_init();vif_init();
  for(unsigned n=0;n<4;n++){put(a,n,0);put(b,n,0);}
  put(a,0,0x01000101); /* STCYCL CL=WL=1 */
  put(a,2,kind?0xca010000:0xec010000); /* I-bit MPG/UNPACK V4-32 */
  put(a,3,0x11223344);
  if(kind){put(b,0,0);put(b,1,0x04000055);}
  else{put(b,0,0x55667788);put(b,1,0x99aabbcc);put(b,2,0xddeeff00);put(b,3,0x04000055);}
  vif1_process_quadwords(1,a,1);assert(!vif1_get_state()->irq_stalled);
  vif1_process_quadwords(1,b,1);
  assert(vif1_get_state()->irq_stalled&&vif1_get_state()->interrupts_raised==1);
  assert(vif1_get_state()->itops!=0x55&&vif1_get_state()->stall_words);
  assert(vif_mmio_write32(0x10003c10,8));vif_service();
  assert(!vif1_get_state()->irq_stalled&&vif1_get_state()->itops==0x55);
 }
 puts("PASS split MPG/UNPACK IRQ preserves following command until STC");return 0;
}
