#include <stdio.h>
#include <string.h>
#include "hw/dma.c"
static unsigned failures;
#define CHECK(x) do{if(!(x)){printf("FAIL line %u: %s\n",__LINE__,#x);failures++;}}while(0)
static uint8_t ram[8192],spr[16384];
static void w32(uint8_t*p,uint32_t w){for(int n=0;n<4;n++)p[n]=w>>(n*8);}
static void reset(void){dma_init();dma_bind_ee_ram(ram,sizeof ram);dma_bind_scratchpad(spr,sizeof spr);memset(ram,0,sizeof ram);memset(spr,0xee,sizeof spr);}
int main(void){
 reset();w32(ram+0x100,0x70000001);w32(ram+0x104,0x12345678);w32(ram+0x108,0xabcdef01);w32(ram+0x10c,0x87654321);
 for(int n=0;n<16;n++)ram[0x110+n]=n+1;
 dma_channel_t *c=&dma_get_state()->chan[9];c->tadr=0x100;c->sadr=0x3ff0;c->chcr=0x145;dma_channel_kick(9);
 CHECK(!memcmp(spr+0x3ff0,ram+0x100,16));CHECK(!memcmp(spr,ram+0x110,16));CHECK(c->sadr==16);CHECK(c->tadr==0x120);CHECK(!(c->chcr&0x100));CHECK(!c->last_error);CHECK(c->quadwords_transferred==2);CHECK(dma_get_state()->d_stat&(1u<<9));
 /* TTE off: no header, only payload, at the original SADR. */
 memset(spr,0xee,sizeof spr);c->tadr=0x100;c->sadr=0x3ff0;c->chcr=0x105;dma_channel_kick(9);
 CHECK(!memcmp(spr+0x3ff0,ram+0x110,16));CHECK(spr[0]==0xee);CHECK(c->sadr==0);
 /* REFE QWC=0 still transfers the complete tag with TTE enabled. */
 w32(ram+0x100,0);w32(ram+0x104,0x1000);c->tadr=0x100;c->sadr=32;c->chcr=0x145;dma_channel_kick(9);
 CHECK(!memcmp(spr+32,ram+0x100,16));CHECK(c->sadr==48);CHECK(!c->last_error);
 printf("%s toSPR TTE: full tag, payload order, scratch wrap, disabled, zero QWC\n",failures?"FAIL":"PASS");return failures!=0;
}
