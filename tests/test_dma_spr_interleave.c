#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "hw/dma.c"
static uint8_t ram[131072],spr[16384],want_ram[131072],want_spr[16384];
static unsigned notified[32],notifications;
static void notify(uint32_t address,uint32_t bytes)
{assert(bytes==16&&notifications<32);notified[notifications++]=address;}
static void init(void)
{
    dma_init();dma_bind_ee_ram(ram,sizeof ram);dma_bind_scratchpad(spr,sizeof spr);
    dma_set_ee_write_notify(notify);notifications=0;
    for(unsigned i=0;i<sizeof ram;i++)ram[i]=(uint8_t)(i*37u+(i>>8));
    for(unsigned i=0;i<sizeof spr;i++)spr[i]=(uint8_t)(i*13u+(i>>7));
    memcpy(want_ram,ram,sizeof ram);memcpy(want_spr,spr,sizeof spr);
}
int main(void)
{
    unsigned cases=0;
    const unsigned counts[]={0,1,2,3,7,17},blocks[]={0,1,2,5,255},skips[]={0,1,3,255};
    for(unsigned direction=0;direction<2;direction++)for(unsigned n=0;n<6;n++)
    for(unsigned b=0;b<5;b++)for(unsigned k=0;k<4;k++)for(unsigned wrap=0;wrap<2;wrap++) {
        init();unsigned count=counts[n],block=blocks[b],skip=skips[k];
        unsigned address=128,sadr=wrap?16368:32,expected_notifications[32],writes=0;
        /* Byte-level oracle with independently calculated group indices. */
        unsigned group=block?block:count;
        for(unsigned q=0;q<count;q++) {
            unsigned dst=128+(q+(q/group)*skip)*16u;
            unsigned so=(sadr+q*16u)&16383u;
            for(unsigned byte=0;byte<16;byte++) {
                if(direction)want_ram[dst+byte]=spr[(so+byte)&16383u];
                else want_spr[(so+byte)&16383u]=ram[dst+byte];
            }
            if(direction)expected_notifications[writes++]=dst;
        }
        unsigned groups=count?(count+group-1u)/group:0;
        address+=(count+groups*skip)*16u;
        int channel=direction?DMA_CHANNEL_FROMSPR:DMA_CHANNEL_TOSPR;
        dma_channel_t *c=&g_dma.chan[channel];
        c->madr=128;c->sadr=sadr;c->qwc=count;c->chcr=0x108;
        g_dma.d_sqwc=(block<<16)|skip;
        g_dma.d_ctrl=direction?(2u<<4):0; /* fromSPR updates STADR. */
        dma_channel_kick(channel);
        assert(!c->last_error&&!c->qwc&&!(c->chcr&0x100));
        assert(c->madr==address&&c->sadr==((sadr+count*16u)&16383u));
        assert(c->quadwords_transferred==count);
        assert(g_dma.d_stat&(1u<<channel));
        assert(!memcmp(ram,want_ram,sizeof ram)&&!memcmp(spr,want_spr,sizeof spr));
        assert(notifications==writes&&!memcmp(notified,expected_notifications,writes*sizeof(unsigned)));
        if(direction)assert(g_dma.d_stadr==address);
        cases++;
    }
    init();dma_channel_t *c=&g_dma.chan[DMA_CHANNEL_FROMSPR];
    c->madr=sizeof ram-32;c->qwc=2;c->sadr=0;c->chcr=0x108;
    g_dma.d_sqwc=(1u<<16)|5u;
    dma_channel_kick(DMA_CHANNEL_FROMSPR);
    assert(c->last_error==DMA_ERR_OUT_OF_BOUNDS&&c->qwc==1&&c->sadr==16);
    assert(c->quadwords_transferred==1&&notifications==1);
    assert(!(g_dma.d_stat&(1u<<DMA_CHANNEL_FROMSPR))&&!(c->chcr&0x100));
    assert(!memcmp(ram+sizeof ram-32,spr,16));
    init();c=&g_dma.chan[DMA_CHANNEL_TOSPR];
    c->madr=sizeof ram-8;c->qwc=1;c->sadr=64;c->chcr=0x100;
    dma_channel_kick(DMA_CHANNEL_TOSPR);
    assert(c->last_error==DMA_ERR_OUT_OF_BOUNDS&&c->qwc==1&&c->sadr==64);
    assert(!(g_dma.d_stat&(1u<<DMA_CHANNEL_TOSPR))&&!(c->chcr&0x100));
    assert(!memcmp(spr,want_spr,sizeof spr));
    init();c=&g_dma.chan[DMA_CHANNEL_FROMSPR];
    c->madr=0x1000;c->qwc=2;c->chcr=0x108;
    g_dma.d_ctrl=13;g_dma.d_rbsr=0x70;g_dma.d_rbor=0x1000;
    dma_channel_kick(DMA_CHANNEL_FROMSPR);
    assert(c->last_error==DMA_ERR_UNSUPPORTED_TAG&&c->qwc==2&&!notifications);
    assert(!memcmp(ram,want_ram,sizeof ram)&&!g_dma.d_stat);
    printf("PASS SPR interleave: %u byte-oracle cases, partial/error completion and MFIFO rejection\n",cases);
    return 0;
}
