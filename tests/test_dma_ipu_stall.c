/* Synthetic producer/consumer transfers. D_CTRL selectors follow Dmac.h:
 * STS=3 fromIPU; STD=1 VIF1, 2 GIF, 3 SIF1. */
#include "core/hw/dma.h"
#include "core/hw/ipu.h"
#include "core/hw/gif.h"
#include "core/hw/vif.h"
#include <stdio.h>
#include <string.h>
static uint8_t ram[8192];
static unsigned checks, failures, notified;
#define CHECK(c) do { checks++; if (!(c)) { if (failures<12) \
 printf("FAIL line %u: %s\n",__LINE__,#c); failures++; } } while (0)
static void put(unsigned off, unsigned value)
{ for(unsigned k=0;k<4;k++)ram[off+k]=value>>(8*k); }
static void reset(void)
{ dma_init(); ipu_init(); gif_init(); vif_init(); memset(ram,0,sizeof ram); dma_bind_ee_ram(ram,sizeof ram); notified=0; }
static void notify(uint32_t addr, uint32_t bytes)
{ CHECK(addr==4096+notified); notified+=bytes; }
static void producer(unsigned sts)
{
 reset(); dma_set_ee_write_notify(notify);
 dma_get_state()->d_ctrl=sts<<4; dma_get_state()->d_stadr=0x1234;
 dma_mmio_write32(0x1000b010,4096);
 dma_mmio_write32(0x1000b020,10);
 dma_mmio_write32(0x1000b000,0x100);
 CHECK(dma_get_state()->d_stadr==(sts==3?4096u:0x1234u));
 CHECK(dma_get_state()->chan[3].qwc==10);
 for(unsigned batch=0;batch<2;batch++) {
  unsigned n=batch?3:7;
  /* Seed the actual IPU FIFO, not a DMA sink stub. */
  ipu_state_t *s=ipu_get_state(); CHECK(s->out_count==0);
  s->out_head=0; s->out_count=n;
  for(unsigned i=0;i<n;i++)memset(s->output[i],batch?0xb2:0xa1,16);
  ipu_service();
  CHECK(dma_get_state()->d_stadr==(sts==3?4096u+(batch?160u:112u):0x1234u));
  CHECK(dma_get_state()->chan[3].qwc==(batch?0u:3u));
  CHECK(notified==(batch?160u:112u));
  CHECK(!!(dma_get_state()->chan[3].chcr&0x100)==!batch);
  CHECK(!!(dma_get_state()->d_stat&8)==!!batch);
 }
 for(unsigned i=0;i<160;i++)CHECK(ram[4096+i]==(i<112?0xa1:0xb2));
 dma_set_ee_write_notify(NULL);
}
static void consumer(unsigned channel,unsigned std)
{
 reset();
 dma_set_sink(channel,channel==DMA_CHANNEL_GIF?gif_process_quadwords:vif1_process_quadwords);
 dma_get_state()->d_ctrl=std<<6; dma_get_state()->d_stadr=4096;
 /* Two empty GIF packets or eight VIF NOPs, followed by END QWC=0. */
 if(channel==DMA_CHANNEL_GIF){put(4096,0x8000);put(4112,0x8000);}
 put(256,0x40000002);put(260,4096);put(272,0x70000000);
 dma_channel_t *c=&dma_get_state()->chan[channel];c->tadr=256;c->chcr=0x105;
 dma_channel_kick(channel);
 unsigned selected=std==(channel==DMA_CHANNEL_GIF?2u:1u);
 if(selected){
  CHECK(c->qwc==2&&c->madr==4096&&(c->chcr&0x100));
  CHECK(dma_get_state()->d_stat&(1u<<13));
  CHECK(!(dma_get_state()->d_stat&(1u<<channel)));
  dma_get_state()->d_stadr=4112; CHECK(dma_gif_service());
  CHECK(c->qwc==1&&c->madr==4112&&(c->chcr&0x100));
  dma_get_state()->d_stadr=4128; CHECK(dma_gif_service());
 }
 /* END QWC=0 still loads its inline payload address into MADR. */
 CHECK(!c->qwc&&c->madr==288&&!(c->chcr&0x100));
 CHECK(c->quadwords_transferred==2&&c->last_error==0);
 CHECK(dma_get_state()->d_stat&(1u<<channel));
 CHECK(selected||!(dma_get_state()->d_stat&(1u<<13)));
}
int main(void)
{
 for(unsigned s=0;s<4;s++)producer(s);
 for(unsigned channel=1;channel<=2;channel++)for(unsigned s=0;s<4;s++)consumer(channel,s);
 printf("IPU producer / VIF1-GIF REFS selectors: %u checks, %u failures\n",checks,failures);
 return failures?1:0;
}
