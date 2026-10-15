#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/hw/ipu.h"
#include "core/hw/arm_worker.h"
static unsigned dma_active,dma_visits,irq_count;
int dma_ipu_work_pending(void){return dma_active;}
int dma_ipu_service(void){dma_visits++;return 0;}
void ee_intc_raise(int irq){assert(irq==8);irq_count++;}
void arm_worker_reset(void){}
int arm_worker_take(uint8_t *a,unsigned b){(void)a;(void)b;return -1;}
int arm_worker_submit_csc(const uint8_t *a,uint32_t b,uint16_t c,uint16_t d){(void)a;(void)b;(void)c;(void)d;return 0;}
void ipu_mpeg_begin(ipu_state_t *s){(void)s;}
int ipu_mpeg_decode(ipu_state_t *s,int(*p)(unsigned,int,uint32_t*),void(*c)(unsigned)){(void)s;(void)p;(void)c;return 0;}
uint32_t ipu_vlc_decode(uint32_t a,unsigned b,uint32_t c,unsigned*d){(void)a;(void)b;(void)c;if(d)*d=0;return 0;}
void ipu_pack_convert(const uint8_t a[1024],uint8_t*b,uint32_t c,uint32_t d,const uint8_t e[32]){(void)a;(void)b;(void)c;(void)d;(void)e;}
void ipu_csc_convert(const uint8_t a[384],uint8_t*b,uint32_t c,uint32_t d,uint16_t e,uint16_t f){(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;}
int main(void){
 ipu_init();for(unsigned n=0;n<1000;n++)ipu_service();
 assert(dma_visits==0); /* cold boot stays on fast path */
 dma_active=1;ipu_service();assert(dma_visits==1);
 dma_active=0;ipu_service();assert(dma_visits==1);
 assert(ipu_input_write(NULL,1)==0);
 assert(ipu_output_read(NULL,1)==0);
 uint8_t quadword[16]={0};assert(ipu_input_write(quadword,1)==1);
 assert(ipu_get_state()->in_count==1);
 uint32_t reg=0;assert(ipu_mmio_read32(0x10002020,&reg));
 assert(reg!=0);
 ipu_mmio_write32(0x10002000,0x00000000); /* BCLR */
 assert(irq_count==1);
 puts("PASS IPU idle shortcut, active DMA service, FIFO guards and BCLR");return 0;
}
