/* DMA GS BUSDIR->VIF1 reverse-transfer bookkeeping regression. */
#include <stdio.h>
#include <string.h>
#include "core/hw/dma.h"
#include "core/hw/gif.h"
#include "core/hw/vif.h"
#include "core/hw/gs.h"
static gif_state_t gif;
static vif_state_t v0,v1;
static gs_state_t gs;
const gif_state_t * const gif_scheduler_view=&gif;
const vif_state_t * const vif0_scheduler_view=&v0;
const vif_state_t * const vif1_scheduler_view=&v1;
static int done_calls, read_calls, allow_read=1;
static uint8_t incoming[16]={0x44,0x33,0x22,0x11,0x88,0x77,0x66,0x55};
void ee_intc_raise_sbus_event(void){}
gif_state_t *gif_get_state(void){return &gif;}
gs_state_t *gs_get_state(void){return &gs;}
vif_state_t *vif0_get_state(void){return &v0;}
vif_state_t *vif1_get_state(void){return &v1;}
void gif_process_quadwords(int c,const uint8_t *p,uint32_t n){(void)c;(void)p;(void)n;}
uint32_t gif_try_process_quadwords(int c,const uint8_t *p,uint32_t n){(void)c;(void)p;(void)n;return 0;}
uint32_t gif_read_local_fifo(uint8_t *p,uint32_t n){
 read_calls++;
 if(!allow_read)return 0;
 if(n){memcpy(p,incoming,16u);return 1;}
 return 0;
}
void gif_fifo_service(void){}
void vif0_process_quadwords(int c,const uint8_t *p,uint32_t n){(void)c;(void)p;(void)n;}
void vif1_process_quadwords(int c,const uint8_t *p,uint32_t n){(void)c;(void)p;(void)n;}
uint32_t vif_accept_quadwords(int c,const uint8_t *p,uint32_t n){(void)c;(void)p;(void)n;return 0;}
void vif_service(void){}
int main(void){
 static uint8_t ram[512];memset(ram,0x5a,sizeof ram);
 dma_init();dma_bind_ee_ram(ram,sizeof ram);
 dma_set_sink(DMA_CHANNEL_VIF1,vif1_process_quadwords);
 dma_state_t *s=dma_get_state();dma_channel_t *ch=&s->chan[DMA_CHANNEL_VIF1];
 ch->madr=64;ch->qwc=1;ch->chcr=0x100;gs.busdir=1;
 /* VIF1_STAT.FDR=0: cannot read GS bus, must not modify RAM. */
 if(dma_gif_service()!=0||memcmp(ram+64,incoming,8)==0||ch->qwc!=1||read_calls)return 1;
 v1.stat|=1u<<23;
 if(dma_gif_service()!=1||ch->qwc||ch->madr!=80||ch->chcr&0x100||
    memcmp(ram+64,incoming,16)||!read_calls||!(s->d_stat&(1u<<DMA_CHANNEL_VIF1)))return 2;
 /* Missing GS FIFO data cannot complete a VIF1 DMA. */
 allow_read=0;ch->madr=128;ch->qwc=1;ch->chcr=0x100;
 if(dma_gif_service()!=0||ch->qwc!=1||!(ch->chcr&0x100)||ram[128]!=0x5a)return 3;
 puts("PASS: VIF1 GS reverse DMA FDR gate, 16-byte read, MADR/QWC, IRQ and no fake completion");
 return 0;
}
