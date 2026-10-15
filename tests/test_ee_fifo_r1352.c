#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "core/ee/ee_core.h"
#include "core/hw/vif.h"
#include "core/hw/vu.h"
#include "core/hw/gif.h"
static void send(ee_state_t *s,uint32_t addr,uint32_t a,uint32_t b,uint32_t c,uint32_t d){
 ee_reg128_t q={0};q.ud0=(uint64_t)a|((uint64_t)b<<32);q.ud1=(uint64_t)c|((uint64_t)d<<32);ee_mem_write128(s,addr,&q);
}
int main(void){
 static ee_state_t s;vif_init();vu1_init();gif_init();
 send(&s,0xb0004000,0,0,0,0x6c010005); /* UNPACK V4-32, one vector at qword5 */
 send(&s,0xb0004000,11,22,33,44);
 assert(s.vu0_mem[80]==0); /* VIF0 writes the live global EE, not this test object */
 assert(ee_core_get_state()->vu0_mem[80]==11&&ee_core_get_state()->vu0_mem[84]==22);
 send(&s,0xb0005000,0,0,0,0x50000002); /* DIRECT: tag plus A+D RGBAQ */
 send(&s,0xb0005000,0x8001,0x10000000,0xe,0);
 send(&s,0xb0005000,0x44332211,0x3f800000,1,0);
 assert(gif_get_state()->rgba==0x44332211);
 assert(vif1_get_state()->direct_qwords_forwarded==2);
 send(&s,0xb0006000,0x8001,0x10000000,0xe,0);
 send(&s,0xb0006000,0x88776655,0x3f800000,1,0);
 assert(gif_get_state()->rgba==0x88776655);
 puts("PASS CPU SQ FIFO writes: VIF0 UNPACK, fragmented VIF1 DIRECT and GIF A+D");return 0;
}
