#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hw/dma.c"
/* Non-GX fixture: the standard DMA chain test uses a mock SIF sink. */
static vif_state_t v0,v1;
static gif_state_t gf;
static gs_state_t gs;
const vif_state_t *const vif0_scheduler_view=&v0;
const vif_state_t *const vif1_scheduler_view=&v1;
const gif_state_t *const gif_scheduler_view=&gf;
vif_state_t *vif0_get_state(void){return &v0;}
vif_state_t *vif1_get_state(void){return &v1;}
gif_state_t *gif_get_state(void){return &gf;}
gs_state_t *gs_get_state(void){return &gs;}
void gif_process_quadwords(int c,const uint8_t*p,uint32_t q){(void)c;(void)p;(void)q;}
void vif0_process_quadwords(int c,const uint8_t*p,uint32_t q){(void)c;(void)p;(void)q;}
void vif1_process_quadwords(int c,const uint8_t*p,uint32_t q){(void)c;(void)p;(void)q;}
void vif_service(void){}
void gif_fifo_service(void){}
uint32_t gif_try_process_quadwords(int a,const uint8_t*b,uint32_t c){(void)a;(void)b;return c;}
uint32_t gif_read_local_fifo(uint8_t*b,uint32_t c){(void)b;(void)c;return 0;}
uint32_t vif_accept_quadwords(int c,const uint8_t*p,uint32_t q){(void)c;(void)p;return q;}
static uint8_t ram[65536],observed[256];static unsigned count;
static void sink(int channel,const uint8_t *src,uint32_t qwc){(void)channel;memcpy(observed+count,src,qwc*16u);count+=qwc*16u;}
static void tag(unsigned at,unsigned id,unsigned qwc,unsigned addr,unsigned marker){
 uint32_t ctrl=(id<<28)|qwc;
 for(unsigned k=0;k<4;k++){ram[at+k]=(uint8_t)(ctrl>>(8*k));ram[at+4+k]=(uint8_t)(addr>>(8*k));}
 if(qwc && id!=DMA_TAG_REF&&id!=DMA_TAG_REFE)memset(ram+at+16,marker,qwc*16u);
}
static int check(int cond,const char*desc){if(!cond){fprintf(stderr,"FAIL %s\n",desc);return 0;}return 1;}
int main(void){
 dma_init();dma_bind_ee_ram(ram,sizeof ram);dma_set_sink(DMA_CHANNEL_SIF0,sink);
 tag(0x1000,DMA_TAG_CALL,1,0x2000,0x11);
 tag(0x2000,DMA_TAG_CALL,1,0x3000,0x22);
 tag(0x3000,DMA_TAG_RET,1,0,0x33);
 tag(0x2020,DMA_TAG_RET,1,0,0x44);
 tag(0x1020,DMA_TAG_END,1,0,0x55);
 dma_channel_t *ch=&dma_get_state()->chan[DMA_CHANNEL_SIF0];ch->tadr=0x1000;
 dma_channel_kick(DMA_CHANNEL_SIF0); /* STR normally set by MMIO */
 ch->chcr=0x104u;dma_channel_kick(DMA_CHANNEL_SIF0);
 if(!check(count==80,"5 inline payloads")||!check(!(ch->chcr&0x100),"END clears STR")||
    !check(((ch->chcr>>4)&3)==0,"call stack depth returns to 0")||
    !check(ch->asr0==0&&ch->asr1==0,"ASR registers popped")||
    !check(ch->last_error==0,"nested CALL/RET no error"))return 1;
 const uint8_t marks[]={0x11,0x22,0x33,0x44,0x55};
 for(unsigned i=0;i<5;i++)for(unsigned j=0;j<16;j++)if(!check(observed[i*16+j]==marks[i],"ordered inline payload"))return 2;
 /* Third nested CALL must fail closed without dispatching its payload. */
 memset(ram,0,sizeof ram);count=0;dma_init();dma_set_sink(DMA_CHANNEL_SIF0,sink);
 tag(0x1000,DMA_TAG_CALL,0,0x2000,0);
 tag(0x2000,DMA_TAG_CALL,0,0x3000,0);
 tag(0x3000,DMA_TAG_CALL,1,0x4000,0x33);
 ch=&dma_get_state()->chan[DMA_CHANNEL_SIF0];ch->tadr=0x1000;ch->chcr=0x104;
 dma_channel_kick(DMA_CHANNEL_SIF0);
 if(!check(ch->last_error==DMA_ERR_UNSUPPORTED_TAG,"overflow declines")||
    !check(count==0,"overflow no payload")||!check(!(ch->chcr&0x100),"overflow stops"))return 3;
 /* Underflow RET transfers its payload then terminates. */
 memset(ram,0,sizeof ram);count=0;dma_init();dma_set_sink(DMA_CHANNEL_SIF0,sink);
 tag(0x5000,DMA_TAG_RET,1,0,0xa5);
 ch=&dma_get_state()->chan[DMA_CHANNEL_SIF0];ch->tadr=0x5000;ch->chcr=0x104;
 dma_channel_kick(DMA_CHANNEL_SIF0);
 if(!check(count==16&&observed[0]==0xa5,"RET underflow ends after payload")||
    !check(!(ch->chcr&0x100),"underflow completion"))return 4;
 puts("PASS DMA nested CALL/RET, overflow, underflow and payload ordering");return 0;
}
