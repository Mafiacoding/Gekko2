/* Admission/level-latch oracle, synthetic NOP bodies; no guest BIOS. */
#include <stdio.h>
#include <string.h>
#include "core/ee/ee_core.c"
#include "core/system.h"
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL line %d status=%x intc=%u dmac=%u armed=%u delay=%u n=%u\n",__LINE__,status,intc,dmac,armed,delay,n);return 1;}}while(0)
int main(void)
{
 bios_image_t bios={0};if(system_init(&bios,&bios))return 1;
 ee_state_t *s=ee_core_get_state();uint32_t words[32]={0};
 const unsigned statuses[]={0,1,0x10000,0x10001,0x10401,0x10801,0x10c01,0x10c03,0x10c05};
 gekko2_optimization_mask|=GEKKO2_OPT_BIT(GEKKO2_OPT_NATIVE_EVENTS)|GEKKO2_OPT_BIT(GEKKO2_OPT_EE_SERVICES);
 for(unsigned k=0;k<sizeof statuses/sizeof *statuses;k++)
 for(unsigned intc=0;intc<2;intc++)for(unsigned dmac=0;dmac<2;dmac++)
 for(unsigned armed=0;armed<2;armed++)for(unsigned delay=0;delay<2;delay++)
 for(unsigned n=8;n<=32;n*=2){
  unsigned status=statuses[k];s->pc=0x80200000;s->next_pc=s->pc+4;
  s->halted=s->idle=s->branch_pending=s->vu0_running=0;
  s->cop0[12]=status;s->cop0[9]=100;s->cop0[11]=10000;s->cop0[13]=0x1234ab00;
  g_ee_irq.intc_armed=armed;g_ee_irq.intc_delay=delay;
  ee_intc_get_state()->stat=intc;ee_intc_get_state()->mask=1;
  dma_state_t *d=dma_get_state();memset(d,0,sizeof *d);d->d_ctrl=1;d->d_stat=dmac?0x00010001:0;
  g_ee_display_ticks=g_ee_display_phase=100000;
  ee_timers_batch_clock[0]=1000;ee_timers_batch_clock[1]=0;
  ee_state_t before=*s;uint32_t oldcause=s->cop0[13];
  int enabled=(status&0x10007u)==0x10001u;
  int allowed=!delay&&intc==armed&&!(enabled&&((intc&&(status&0x400))||(dmac&&(status&0x800))));
  unsigned got=ee_core_native_window_begin_impl(s,s->pc,words,n,n,0,1);
  CHECK(got==(allowed?n:0));
  if(!allowed){CHECK(!memcmp(s,&before,sizeof before));CHECK(g_ee_display_ticks==100000);}
  else{
   unsigned expected=(oldcause&~0xc00u)|(intc?0x400:0)|(dmac?0x800:0);
   CHECK(s->cop0[13]==expected);CHECK(s->cop0[9]==100+n);
   CHECK(s->instructions_executed==before.instructions_executed+n);
   CHECK(g_ee_irq.intc_armed==armed&&g_ee_irq.intc_delay==delay);
   CHECK(ee_timers_batch_clock[0]==1000-n);
  }
 }
 printf("PASS %u masked-level window admission/Cause/Count/retirement/timer/decline checks\n",checks);
 return 0;
}
