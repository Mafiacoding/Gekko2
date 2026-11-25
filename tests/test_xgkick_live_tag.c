#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/hw/vu.h"
#include "core/hw/gif.h"
#include "core/hw/gs_mem.h"
#include "core/ee/ee_core.h"
static void put(uint8_t *p,uint32_t w){for(unsigned i=0;i<4;i++)p[i]=w>>(8*i);}
static void tag(unsigned addr,unsigned loops){vu1_state_t*v=vu1_get_state();memset(v->mem+addr,0,16);put(v->mem+addr,loops|0x8000);put(v->mem+addr+4,1u<<28);put(v->mem+addr+8,14);}
static void payload(unsigned addr,unsigned val){vu1_state_t*v=vu1_get_state();memset(v->mem+addr,0,16);put(v->mem+addr,val);put(v->mem+addr+8,GS_REG_XYOFFSET_1);}
static void step(uint32_t lo){vu1_state_t*v=vu1_get_state();unsigned pc=v->tpc&16383;put(v->micro+pc,lo);put(v->micro+pc+4,0x2ff);assert(!vu_micro_step_pipeline(v->vf,v->vi,v->acc,v->mem,16383,v->micro,16383,&v->tpc,&v->branch_delay,&v->branch_target,&v->ebit_delay,&v->instructions_executed,&v->unimplemented_opcodes_seen,&v->pipeline));}
int main(void){
 for(unsigned wrap=0;wrap<2;wrap++)for(unsigned accepted=0;accepted<2;accepted++){
  vu1_init();gif_init();gs_mem_init();vu1_state_t*v=vu1_get_state();gif_state_t*g=gif_get_state();
  unsigned addr=wrap?16368:0;tag(addr,accepted?2:1);payload((addr+16)&16383,0x1111);payload((addr+32)&16383,0x2222);
  v->vi[1]=addr/16;g->gif_ctrl=accepted?0:8;step(0x80000efc);step(0x800003bf);
  /* Rejected tag remains live; accepted tag is an immutable parser decision. */
  tag(addr,accepted?1:2);g->gif_ctrl=0;vu1_tick();
  assert(!v->xgkick_pending);assert(!g->register_carry_path[GIF_PATH_1].remaining);
  assert(g->ctx1_xyoffset_x==0x2222);assert(g->gif_path1_transfers==1);
 }
 puts("PASS 4 XGKICK live/rejected versus accepted tag and ring-wrap cases");return 0;
}
