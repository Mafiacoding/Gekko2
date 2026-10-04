#include <stdio.h>
#include <string.h>
#include "hw/dma.c"
static uint8_t ram[8192],spr[16384],capture[64];static unsigned captured,fail;
#define C(x) do{if(!(x)){printf("FAIL %s\n",#x);fail++;}}while(0)
static void w32(uint8_t*p,uint32_t x){for(int i=0;i<4;i++)p[i]=x>>(i*8);}
static void sink(int ch,const uint8_t*p,uint32_t n){(void)ch;if(captured+n*16<=sizeof(capture)){memcpy(capture+captured,p,n*16);captured+=n*16;}}
static void init(int channel,uint32_t start){dma_init();dma_bind_ee_ram(ram,sizeof(ram));dma_bind_scratchpad(spr,sizeof(spr));memset(ram,0,sizeof(ram));memset(spr,0,sizeof(spr));captured=0;g_dma.d_ctrl=channel==1?9:13;g_dma.d_rbsr=0x70;g_dma.d_rbor=0x1000;g_dma.chan[8].madr=start;g_dma.chan[channel].tadr=start;g_dma.chan[channel].chcr=0x104;dma_set_sink(channel,sink);}
static void produce(unsigned qwc){g_dma.chan[8].qwc=qwc;g_dma.chan[8].chcr=0x100;dma_channel_kick(8);}
int main(void){
init(1,0x1000);dma_channel_kick(1);C(g_dma.chan[1].tadr==0x1000);C(g_dma.chan[1].chcr&0x100);C(!(g_dma.d_stat&2));C(!captured);
w32(spr,0x70000001);for(int i=0;i<16;i++)spr[16+i]=i+1;
produce(1);C(g_dma.chan[1].chcr&0x100);C(g_dma.chan[1].tadr==0x1000);C(!captured);C(!(g_dma.d_stat&2));
produce(1);C(!(g_dma.chan[1].chcr&0x100));C(g_dma.d_stat&2);C(captured==16);C(!memcmp(capture,spr+16,16));
init(2,0x1070);dma_channel_kick(2);w32(spr,0x70000001);memset(spr+16,0xa5,16);produce(2);
C(g_dma.chan[8].madr==0x1010);C(g_dma.chan[2].tadr==0x1010);C(captured==16);C(capture[0]==0xa5);C(!(g_dma.chan[2].chcr&0x100));C(g_dma.d_stat&4);
/* CNT then an unavailable next tag must stall without completing. */
init(1,0x1000);w32(spr,0x10000000);produce(1);dma_channel_kick(1);C(g_dma.chan[1].chcr&0x100);C(g_dma.chan[1].tadr==0x1010);C(!(g_dma.d_stat&2));
printf("%s MFIFO: empty drain, incomplete packet, producer wakeup, wrapped payload, next-tag stall\n",fail?"FAIL":"PASS");return fail?1:0;}
