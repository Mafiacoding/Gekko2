/* GPL-3.0-or-later. All pipeline work uses the production one-pair entry. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/hw/vu.h"
#include "core/hw/vif.h"
#include "core/hw/ee_intc.h"
#include "core/ee/ee_core.h"
#include "core/recompiler/optimization.h"
#include "vu_aot_test_fixture.h"
typedef struct {uint32_t vf[32][4],vi[32],acc[4],pc,bd,bt,ed;uint64_t retired,unknown;vu_pipeline_t pipeline;uint8_t mem[16384],micro[16384];vu_timing_t timing;} context;
static context start,reference,current;
static void put(uint8_t *p,uint32_t w){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(w>>(8*i));}
static void run(context *c,unsigned mask,unsigned timed){
 for(unsigned i=0;i<3;i++) {
  int stopped=timed?vu_micro_step_timed(c->vf,c->vi,c->acc,c->mem,mask,c->micro,mask,&c->pc,&c->bd,&c->bt,&c->ed,&c->retired,&c->unknown,&c->pipeline,&c->timing):
   vu_micro_step_pipeline(c->vf,c->vi,c->acc,c->mem,mask,c->micro,mask,&c->pc,&c->bd,&c->bt,&c->ed,&c->retired,&c->unknown,&c->pipeline);
  if(stopped)break;
 }
}
static void reset_side_effects(void){vu1_init();vif_init();ee_intc_init();memset(ee_core_get_state()->cop2_ctrl,0,sizeof(ee_core_get_state()->cop2_ctrl));}
int main(void)
{
 unsigned comparisons=0;
 for(unsigned i=0;i<vu_aot_fixture_count;i++)for(unsigned unit=0;unit<2;unit++)for(unsigned variant=0;variant<4;variant++)for(unsigned timed=0;timed<2;timed++){
  memset(&start,0,sizeof start);start.vf[0][3]=0x3f800000u;
  for(unsigned r=1;r<32;r++){start.vi[r]=r;for(unsigned lane=0;lane<4;lane++)start.vf[r][lane]=0x3f000000u+(r+lane)*0x10000u;}
  for(unsigned lane=0;lane<4;lane++)start.acc[lane]=0x3f800000u;
  for(unsigned n=0;n<sizeof start.mem;n++)start.mem[n]=(uint8_t)n;
  start.vi[21]=0x3f800000u;start.vi[22]=0x40000000u;start.vi[23]=0x3f000000u;
  start.pipeline.cycle=4;start.pipeline.q_ready=11;start.pipeline.q_value=0x40800000u;start.pipeline.q_pending=variant==1;
  start.pipeline.p_ready=13;start.pipeline.p_value=0x3f800000u;start.pipeline.p_pending=variant==2;
  if(variant==3){start.bd=1;start.bt=16;start.ed=2;}
  uint32_t upper=vu_aot_fixtures[i][0]|(variant==2?0x80000000u:0u),lower=vu_aot_fixtures[i][1];
  put(start.micro,lower);put(start.micro+4,upper);put(start.micro+8,0x800003bfu);put(start.micro+12,0x400002ffu);put(start.micro+16,0x800003bfu);put(start.micro+20,0x000002ffu);
  reference=start;current=start;
  reset_side_effects();gekko2_optimization_mask&=~GEKKO2_OPT_BIT(GEKKO2_OPT_VU_JIT);run(&reference,unit?16383u:4095u,timed);
  vu1_state_t side=*vu1_get_state();uint32_t controls[32];memcpy(controls,ee_core_get_state()->cop2_ctrl,sizeof controls);
  reset_side_effects();gekko2_optimization_mask|=GEKKO2_OPT_BIT(GEKKO2_OPT_VU_JIT);run(&current,unit?16383u:4095u,timed);
  if(memcmp(&reference,&current,sizeof current)){fprintf(stderr,"VU AOT mismatch upper=%08x lower=%08x unit=%u variant=%u\n",upper,lower,unit,variant);return 1;}
  assert(!memcmp(&side,vu1_get_state(),sizeof side));assert(!memcmp(controls,ee_core_get_state()->cop2_ctrl,sizeof controls));comparisons++;
 }
 assert(vu_aot_executed_count(0)>0&&vu_aot_executed_count(1)>0);
 printf("PASS VU AOT: %u VU0/VU1 pipeline comparisons with timed FMAC/Q/P, I/E flags, branch delays and side effects\n",comparisons);return 0;
}
