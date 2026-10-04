#include <stdio.h>
#include <string.h>
#include "hw/dma.c"
static uint8_t ram[4096],scratch[16384];
static int failed;
#define C(x) do{if(!(x)){printf("FAIL: %s\n",#x);failed++;}}while(0)
static void tag(uint8_t*p,uint32_t v){p[0]=v;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}
int main(void){
 dma_init();dma_bind_ee_ram(ram,sizeof(ram));dma_bind_scratchpad(scratch,sizeof(scratch));
 for(int i=0;i<48;i++)ram[256+i]=i+1;
 dma_channel_t *to=&g_dma.chan[DMA_CHANNEL_TOSPR];
 to->madr=256;to->qwc=2;to->sadr=16368;to->chcr=0x100;dma_channel_kick(DMA_CHANNEL_TOSPR);
 C(!memcmp(scratch+16368,ram+256,16));C(!memcmp(scratch,ram+272,16));C(to->sadr==16);C(to->madr==288);C(to->qwc==0);C(g_dma.d_stat&(1u<<9));
 dma_channel_t *from=&g_dma.chan[DMA_CHANNEL_FROMSPR];from->madr=512;from->sadr=16368;from->qwc=2;from->chcr=0x100;dma_channel_kick(DMA_CHANNEL_FROMSPR);
 C(!memcmp(ram+512,ram+256,32));C(from->sadr==16);C(from->madr==544);C(g_dma.d_stat&(1u<<8));
 /* A real toSPR source-chain END tag carries inline payload. */
 tag(ram+1024,0x70000001);memcpy(ram+1040,ram+288,16);
 to->tadr=1024;to->sadr=32;to->chcr=0x104;dma_channel_kick(DMA_CHANNEL_TOSPR);
 C(!memcmp(scratch+32,ram+288,16));C(to->sadr==48);
 memset(scratch+64,0x55,16);to->madr=4090;to->qwc=1;to->sadr=64;to->chcr=0x100;dma_channel_kick(DMA_CHANNEL_TOSPR);
 C(to->last_error==DMA_ERR_OUT_OF_BOUNDS);C(scratch[64]==0x55);C(to->sadr==64);
 printf("%s SPR DMA copy, wrap, source chain and bounds\n",failed?"FAIL":"PASS");return failed?1:0;
}
