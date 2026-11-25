/* Real VIF1 -> GIF PATH2 packets, split across input quadwords. */
#include "core/hw/vif.h"
#include "core/hw/gif.h"
#include "core/hw/gs.h"
#include "core/hw/ee_intc.h"
#include <stdio.h>
#include <string.h>
static unsigned checks,failures;
#define CHECK(c) do{checks++;if(!(c)){printf("FAIL %u: %s\n",__LINE__,#c);failures++;}}while(0)
static void put(uint8_t *p,unsigned v){for(unsigned i=0;i<4;i++)p[i]=v>>(i*8);}
static void reset(void){gif_init();vif_init();gs_init();ee_intc_init();}
int main(void)
{
 uint8_t stream[64]={0};
 put(stream,0x50000003);put(stream+4,0x8002);put(stream+8,0x10000000);put(stream+12,0xe);
 put(stream+20,0x123);put(stream+28,0); /* PRIM */
 put(stream+36,0x80402010);put(stream+44,1); /* RGBAQ */
 put(stream+52,0x04000035); /* ITOP after DIRECT */
 reset();
 for(unsigned i=0;i<3;i++)CHECK(vif_accept_quadwords(1,stream+i*16,1)==1);
 CHECK(gif_get_state()->prim==0x123); /* draw state visible before final payload QWC */
 CHECK(vif1_get_state()->direct_needed_words==12);
 CHECK(vif1_get_state()->direct_qwords_forwarded==2);
 CHECK(vif1_get_state()->itops==0);
 vif_service();CHECK(vif1_get_state()->direct_qwords_forwarded==2);
 CHECK(vif_accept_quadwords(1,stream+48,1)==1);
 CHECK(gif_get_state()->rgba==0x80402010);
 CHECK(!vif1_get_state()->direct_needed_words&&vif1_get_state()->direct_qwords_forwarded==3);
 CHECK(vif1_get_state()->itops==0x35);
 /* Every word split, including 64-bit tag payload boundaries. */
 for(unsigned split=0;split<=16;split++){
  reset();vif1_process_tag_words(1,stream,split);
  unsigned payload=split?split-1:0;if(payload>12)payload=12;
  CHECK(vif1_get_state()->direct_qwords_forwarded==payload/4);
  if(split>=9)CHECK(gif_get_state()->prim==0x123);
  vif1_process_tag_words(1,stream+split*4,16-split);
  CHECK(vif1_get_state()->direct_qwords_forwarded==3&&!vif1_get_state()->direct_needed_words);
  CHECK(gif_get_state()->rgba==0x80402010&&vif1_get_state()->itops==0x35);
 }
 /* DIRECT's I bit must not raise before its final quadword is accepted. */
 reset();put(stream,0xd0000003);
 for(unsigned i=0;i<3;i++)vif_accept_quadwords(1,stream+i*16,1);
 CHECK(ee_intc_get_raise_count(5)==0&&!vif1_get_state()->irq_stalled);
 vif_accept_quadwords(1,stream+48,1);
 CHECK(ee_intc_get_raise_count(5)==1&&vif1_get_state()->irq_stalled);
 /* Second SIGNAL retains exactly its QWC and the following VIFcode. */
 reset();put(stream,0x50000003);
 put(stream+20,1);put(stream+24,0xffffffff);put(stream+28,0x60);
 put(stream+36,2);put(stream+40,0xffffffff);put(stream+44,0x60);
 for(unsigned i=0;i<4;i++)vif_accept_quadwords(1,stream+i*16,1);
 CHECK(vif1_get_state()->direct_sent_words==8&&vif1_get_state()->direct_needed_words==12);
 CHECK(vif1_get_state()->itops==0&&vif1_get_state()->direct_qwords_forwarded==2);
 vif_service();CHECK(vif1_get_state()->direct_qwords_forwarded==2);
 gs_mmio_write64(0x12001000,1);vif_service();
 CHECK(!vif1_get_state()->direct_needed_words&&vif1_get_state()->direct_qwords_forwarded==3);
 CHECK(vif1_get_state()->itops==0x35&&gif_get_state()->siglblid_sigid==2);
 /* IMM=0 is 65536 QWC, but complete arriving QWCs must stream now. */
 reset();memset(stream,0,sizeof stream);put(stream,0x50000000);put(stream+4,0x8000);
 vif_accept_quadwords(1,stream,1);vif_accept_quadwords(1,stream+16,1);
 CHECK(vif1_get_state()->direct_needed_words==65536u*4u);
 CHECK(vif1_get_state()->direct_qwords_forwarded==1);
 CHECK(vif1_get_state()->direct_have_words==7&&vif1_get_state()->direct_sent_words==4);
 printf("VIF DIRECT streaming, partial-QWC retention and completion IRQ: %u checks, %u failures\n",checks,failures);
 return failures?1:0;
}
