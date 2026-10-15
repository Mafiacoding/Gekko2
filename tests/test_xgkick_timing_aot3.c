/* GPL-3.0-or-later. Independent VU issue-clock and GIF boundary oracles. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/hw/vu.h"
#include "core/hw/gif.h"
#include "core/hw/gs_mem.h"
#include "core/ee/ee_core.h"
#include "core/recompiler/optimization.h"
static void put(uint8_t *p,uint32_t w){for(unsigned i=0;i<4;i++)p[i]=w>>(8*i);}
static const uint32_t kick=0x80000efcu; /* XGKICK VI1 */
static void packet(uint32_t addr,uint32_t value) {
    vu1_state_t *v=vu1_get_state();
    memset(v->mem+addr,0,16);
    put(v->mem+addr,0x8001u);put(v->mem+addr+4,1u<<28);put(v->mem+addr+8,14u);
    addr=(addr+16u)&16383u;memset(v->mem+addr,0,16);
    put(v->mem+addr,value);put(v->mem+addr+8,GS_REG_XYOFFSET_1);
}
static int step(uint32_t lower,uint32_t upper) {
    vu1_state_t *v=vu1_get_state();unsigned off=v->tpc&16383u;
    put(v->micro+off,lower);put(v->micro+off+4,upper);
    return vu_micro_step_pipeline(v->vf,v->vi,v->acc,v->mem,16383u,
        v->micro,16383u,&v->tpc,&v->branch_delay,&v->branch_target,&v->ebit_delay,
        &v->instructions_executed,&v->unimplemented_opcodes_seen,&v->pipeline);
}
static void reset(void){vu1_init();gif_init();gs_mem_init();memset(ee_core_get_state()->cop2_ctrl,0,sizeof ee_core_get_state()->cop2_ctrl);}
int main(void) {
    gekko2_optimization_mask&=~GEKKO2_OPT_BIT(GEKKO2_OPT_VU_JIT);
    reset();vu1_state_t *v=vu1_get_state();gif_state_t *g=gif_get_state();
    v->vi[1]=0;packet(0,0x1111);
    assert(!step(kick,0x2ff));
    assert(v->xgkick_pending && v->xgkick_addr==0);
    assert(ee_core_get_state()->cop2_ctrl[29]&(1u<<12));
    assert(g->ctx1_xyoffset_x==0 && g->gif_path1_transfers==1);
    assert(!step(0x800003bf,0x2ff));assert(v->xgkick_addr==16);
    /* The source is still live after its tag left VU memory. */
    put(v->mem+16,0x2222);
    assert(!step(0x800003bf,0x2ff));assert(g->ctx1_xyoffset_x==0);
    assert(!step(0x800003bf,0x2ff));
    assert(g->ctx1_xyoffset_x==0x2222 && !v->xgkick_pending);
    assert(!(ee_core_get_state()->cop2_ctrl[29]&(1u<<12)));
    assert(g->gif_path1_transfers==1);

    reset();v->vi[1]=1023;packet(16368,0x3456);
    step(kick,0x2ff);step(0x800003bf,0x2ff);
    assert(v->xgkick_addr==0 && v->xgkick_pending);
    step(0x800003bf,0x2ff);step(0x800003bf,0x2ff);
    assert(g->ctx1_xyoffset_x==0x3456 && !v->xgkick_pending);

    reset();v->vi[1]=0;packet(0,0x1234);packet(32,0x5678);
    step(kick,0x2ff);v->vi[1]=2;step(kick,0x2ff);
    assert(g->ctx1_xyoffset_x==0x1234 && v->xgkick_pending);
    assert(v->xgkick_busy_stalls==1 && g->gif_path1_transfers==2);
    for(unsigned n=0;n<3;n++)step(0x800003bf,0x2ff);
    assert(g->ctx1_xyoffset_x==0x5678 && !v->xgkick_pending);

    /* Actual runner: E plus one delay pair must flush the remaining packet. */
    reset();v->vi[1]=0;packet(0,0x6789);
    put(v->micro,kick);put(v->micro+4,0x400002ff);
    put(v->micro+8,0x800003bf);put(v->micro+12,0x2ff);
    vu1_exec_micro(0);
    assert(g->ctx1_xyoffset_x==0x6789 && !v->xgkick_pending);
    assert(v->instructions_executed==2 && g->gif_path1_transfers==1);

    /* A no-EOP oversized BIOS packet is bounded to one physical ring. */
    reset();put(v->mem,0x7fff);put(v->mem+4,0); /* NREG=0 means 16 */
    put(v->micro,kick);put(v->micro+4,0x400002ff);
    put(v->micro+8,0x800003bf);put(v->micro+12,0x2ff);
    vu1_exec_micro(0);
    assert(!v->xgkick_pending && !g->register_carry_path[GIF_PATH_1].remaining);
    assert(v->pipeline.cycle<=2050u && g->gif_path1_transfers==1);
    /* BETA12.14: GIF arbitration prevents forced drain.  A subsequent
     * XGKICK must retain the first packet and queue the next start. */
    reset();v->vi[1]=0;packet(0,0x1234);packet(32,0x9876);
    g->gif_ctrl|=8u; /* GIF pause / backpressure */
    step(kick,0x2ff);v->vi[1]=2;
    unsigned before=v->instructions_executed;
    assert(step(kick,0x2ff)==2); /* precise producer stall */
    assert(v->instructions_executed==before && v->xgkick_addr==0);
    assert(v->xgkick_pending && !v->xgkick_deferred);
    assert(g->gif_path1_transfers==1);
    g->gif_ctrl&=~8u;
    assert(!step(kick,0x2ff)); /* retire only after first packet drains */
    vu1_tick();
    assert(!v->xgkick_pending && !v->xgkick_deferred);
    assert(g->gif_path1_transfers==2 && g->ctx1_xyoffset_x==0x9876);
    puts("PASS XGKICK two-cycle clock, live source, wrap, second kick, E drain, BIOS bound");
    return 0;
}
