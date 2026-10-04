#include <stdio.h>
#include <string.h>
#include "core/hw/dma.h"
#include "core/hw/vif.h"
#include "core/hw/vu.h"
#include "core/hw/gif.h"
static unsigned char ram[128];
static void put(unsigned a,unsigned v){ram[a]=v;ram[a+1]=v>>8;ram[a+2]=v>>16;ram[a+3]=v>>24;}
static void reset(void){memset(ram,0,sizeof(ram));dma_init();vif_init();vu1_init();gif_init();dma_bind_ee_ram(ram,sizeof(ram));dma_set_sink(1,vif1_process_quadwords);dma_set_tag_sink(1,vif1_process_tag_words);}
static void kick(unsigned flags){dma_channel_t*c=&dma_get_state()->chan[1];c->tadr=0;c->chcr=0x105|flags;dma_channel_kick(1);}
int main(void){
 reset();put(0,0x70000001);put(12,0x4a010010);put(16,0x12345678);put(20,0x400002ff);kick(0x40);
 if(memcmp(vu1_get_state()->micro+0x80,ram+16,8)!=0||vif1_get_state()->mpg_words_written!=2){puts("FAIL MPG command from DMA tag");return 1;}
 reset();put(0,0x70000000);put(8,0x03000040);put(12,0x02000020);kick(0x40);
 if(vif1_get_state()->base!=0x40||vif1_get_state()->ofst!=0x20){puts("FAIL zero-QWC tag commands");return 2;}
 reset();put(0,0x70000002);put(12,0x50000002);put(16,0x8001);put(20,0x10000000);put(24,0x0e);put(32,6);put(40,0);kick(0x40);
 if(gif_get_state()->prim!=6||vif1_get_state()->direct_qwords_forwarded!=2){puts("FAIL DIRECT command from DMA tag");return 3;}
 reset();
 /* A DIRECT payload is split at both word and DMA boundaries; its
  * apparent VIF command bits must never be interpreted as commands. */
 put(0,0x50000002);put(4,0x8001);put(8,0x10000000);put(12,0x0e);
 put(20,0x03000007);put(28,0);put(36,0x03000055);
 vif1_process_tag_words(1,ram,2);
 vif1_process_quadwords(1,ram+8,1);
 vif1_process_tag_words(1,ram+24,2);
 vif1_process_tag_words(1,ram+32,2);
 if(gif_get_state()->prim!=0x03000007||vif1_get_state()->base!=0x55||vif1_get_state()->direct_needed_words){puts("FAIL split DIRECT payload followed by BASE");return 5;}
 reset();put(0,0x70000000);put(12,0x03000040);kick(0);
 if(vif1_get_state()->base!=0){puts("FAIL TTE disabled");return 4;}
 reset();put(0,0x30000000);put(4,0x03000011);put(8,2);put(12,3);put(16,4);put(20,0x03000055);
 vif1_process_tag_words(1,ram,2);
 vif1_process_quadwords(1,ram+8,1);
 if(vif1_get_state()->row[0]!=0x03000011||vif1_get_state()->row[3]!=4||vif1_get_state()->base!=0x55||vif1_get_state()->register_pending_cmd){puts("FAIL split STROW and following command");return 6;}
 reset();vif1_get_state()->base=0x40;vif1_get_state()->ofst=0x20;vif1_get_state()->tops=0x40;vif1_get_state()->itops=0x33;
 vu1_micro_write32(0,0x80000000u|(1u<<16)|(0x1au<<6)|0x3c);vu1_micro_write32(4,0x000002ff);
 vu1_micro_write32(8,0x80000000u|(2u<<16)|(0x1au<<6)|0x3d);vu1_micro_write32(12,0x400002ff);
 vu1_micro_write32(16,0);vu1_micro_write32(20,0x000002ff);
 put(0,0x14000000);put(4,0);vif1_process_tag_words(1,ram,2);
 if(vu1_get_state()->vi[1]!=0x40||vu1_get_state()->vi[2]!=0x33||vif1_get_state()->tops!=0x60||!vif1_get_state()->dbf){puts("FAIL TOP/ITOP latching and XTOP/XITOP");return 7;}
 vif1_process_tag_words(1,ram,2);
 if(vu1_get_state()->vi[1]!=0x60||vif1_get_state()->tops!=0x40||vif1_get_state()->dbf){puts("FAIL second VU double buffer");return 8;}
 puts("PASS DMA tag VIF commands, MPG payload, DIRECT payload, zero QWC and TTE disabled");return 0;
}
