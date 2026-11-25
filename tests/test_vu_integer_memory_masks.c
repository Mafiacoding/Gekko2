#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "core/hw/vu.h"
static void put(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=v>>(i*8);}
static uint32_t get(const uint8_t*p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void step(vu1_state_t*v,uint32_t w){put(v->micro,w);put(v->micro+4,0x2ff);v->tpc=0;assert(vu_micro_step_pipeline(v->vf,v->vi,v->acc,v->mem,16383,v->micro,16383,&v->tpc,&v->branch_delay,&v->branch_target,&v->ebit_delay,&v->instructions_executed,&v->unimplemented_opcodes_seen,&v->pipeline)==0);}
int main(void){unsigned checks=0;
 for(unsigned relative=0;relative<2;relative++)for(unsigned store=0;store<2;store++)for(unsigned mask=0;mask<16;mask++)for(unsigned alias=0;alias<2;alias++){
  vu1_init();vu1_state_t*v=vu1_get_state();unsigned rs=1,rt=alias?1:2;v->vi[rs]=1023;if(!alias)v->vi[rt]=0xabcd;
  unsigned offset=relative?16368:0;uint32_t value=v->vi[rt];for(unsigned l=0;l<4;l++)put(v->mem+offset+4*l,0xace00000u+0x1111u*(l+1));
  uint8_t before[16];memcpy(before,v->mem+offset,16);
  uint32_t word=relative?(0x800003feu|(store?1u:0u))|rt<<16|rs<<11:((4u+store)<<25)|rt<<16|rs<<11|1u;
  step(v,word|mask<<21);int last=-1;for(unsigned l=0;l<4;l++)if(mask&(8u>>l))last=(int)l;
  if(store){for(unsigned l=0;l<4;l++)assert(get(v->mem+offset+4*l)==((mask&(8u>>l))?value:get(before+4*l)));}
  else {assert(v->vi[rt]==(last<0?value:(get(before+4*last)&65535)));assert(!memcmp(before,v->mem+offset,16));}
  checks++;
 }
 printf("PASS %u independent VU ILW/ISW/ILWR/ISWR lane-mask, zero-mask, alias and wrap cases\n",checks);
}
