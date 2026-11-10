#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/hw/gif.h"
#include "core/hw/gs.h"
#include "core/hw/dma.h"
#include "core/hw/ee_intc.h"
#include "core/hw/gs_mem.h"
#include "core/hw/vu.h"
#include "core/ee/ee_core.h"
#include "core/recompiler/optimization.h"
static void put(uint8_t *p,uint32_t w)
{for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(w>>(8*i));}
static void packet(uint8_t p[64])
{
    memset(p,0,64);
    put(p,0x8003);put(p+4,1u<<28);put(p+8,14);
    put(p+16,0x1111);put(p+20,0xffff);put(p+24,GS_REG_SIGNAL);
    put(p+32,0x22220000);put(p+36,0xffff0000);put(p+40,GS_REG_SIGNAL);
    put(p+48,0x87654321);put(p+52,0xffffffff);put(p+56,GS_REG_LABEL);
}
static void reset(void)
{gs_init();gif_init();gs_mem_init();ee_intc_init();}
int main(void)
{
    uint8_t p[64];packet(p);
    for(int path=0;path<3;path++) {
        reset();gs_mmio_write64(0x12001010,0); /* GS events unmasked. */
        assert(gif_try_process_quadwords(path,p,4)==2);
        gif_state_t *g=gif_get_state();
        assert(g->register_carry_path[path].remaining==2);
        assert(g->siglblid_sigid==0x1111&&!g->siglblid_lblid);
        assert(gs_get_state()->siglblid==0x1111);
        assert(gs_get_state()->csr&1u);
        assert(ee_intc_get_raise_count(0)==1);
        assert(gif_signal_stall_count()==1);
        assert(!gif_try_process_quadwords(path,p+32,2));
        assert(g->register_carry_path[path].remaining==2&&g->siglblid_sigid==0x1111);
        assert(ee_intc_get_raise_count(0)==1);
        gs_mmio_write64(0x12001000,1); /* Acknowledge only SIGNAL. */
        assert(gif_try_process_quadwords(path,p+32,2)==2);
        assert(g->siglblid_sigid==0x22221111&&g->siglblid_lblid==0x87654321);
        uint64_t value=0;assert(gs_mmio_read64(0x12001080,&value));
        assert(value==UINT64_C(0x8765432122221111));
        assert(ee_intc_get_raise_count(0)==2);
        assert(!g->register_carry_path[path].remaining&&!g->arb_owner);
    }
    /* Actual normal GIF DMA must preserve the stalled source position. */
    uint8_t ram[128]={0};memcpy(ram,p,64);
    reset();dma_init();dma_bind_ee_ram(ram,sizeof ram);
    dma_set_sink(DMA_CHANNEL_GIF,gif_process_quadwords);
    dma_channel_t *c=&dma_get_state()->chan[DMA_CHANNEL_GIF];
    c->madr=0;c->qwc=4;c->chcr=0x101;
    dma_channel_kick(DMA_CHANNEL_GIF);
    assert(c->madr==32&&c->qwc==2&&(c->chcr&0x100));
    assert(!(dma_get_state()->d_stat&(1u<<DMA_CHANNEL_GIF)));
    dma_gif_service();assert(c->madr==32&&c->qwc==2);
    gs_mmio_write64(0x12001000,1);dma_gif_service();
    assert(c->madr==64&&!c->qwc&&!(c->chcr&0x100));
    assert(dma_get_state()->d_stat&(1u<<DMA_CHANNEL_GIF));
    /* Masked interrupts still set SIGNAL and impose backpressure. */
    assert(!ee_intc_get_raise_count(0));
    /* Real VU1 E-drain retains the live XGKICK source while SIGNAL stalls. */
    reset();vu1_init();
    gekko2_optimization_mask&=~GEKKO2_OPT_BIT(GEKKO2_OPT_VU_JIT);
    vu1_state_t *v=vu1_get_state();memcpy(v->mem,p,64);v->vi[1]=0;
    put(v->micro,0x80000efcu);put(v->micro+4,0x400002ffu);
    put(v->micro+8,0x800003bfu);put(v->micro+12,0x2ffu);
    vu1_exec_micro(0);
    assert(v->xgkick_pending&&v->xgkick_addr==32);
    assert(gif_get_state()->siglblid_sigid==0x1111);
    gs_mmio_write64(0x12001000,1);
    for(unsigned n=0;n<8&&v->xgkick_pending;n++)vu1_tick();
    assert(!v->xgkick_pending&&gif_get_state()->siglblid_sigid==0x22221111);
    assert(gif_get_state()->siglblid_lblid==0x87654321);
    assert(!(ee_core_get_state()->cop2_ctrl[29]&(1u<<12)));
    puts("PASS SIGNAL PATH1/2/3 stall/retry, SIGLBLID MMIO, IRQ counts, DMA and VU1 E-drain continuation");
    return 0;
}
