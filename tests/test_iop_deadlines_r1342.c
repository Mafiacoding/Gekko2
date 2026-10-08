/* Differential THREADMAN deadlines; real HLE calls, no forced guest progress. */
#include "hw/iop_hle_thread.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static uint64_t signatures[12000];
static uint64_t hash(const void *p,size_t n,uint64_t h)
{const unsigned char *b=p;while(n--){h^=*b++;h*=1099511628211ull;}return h;}
static void invoke(iop_state_t *s,uint32_t pc,uint32_t a,uint32_t b,uint32_t c)
{s->gpr[4]=a;s->gpr[5]=b;s->gpr[6]=c;s->gpr[31]=0x1234;assert(iop_hle_thread_try_handle(s,pc));}
int main(void)
{
 iop_state_t s;uint32_t initial=gekko2_optimization_mask;
 for(unsigned fast=0;fast<2;fast++) {
  memset(&s,0,sizeof s);iop_hle_thread_init();
  gekko2_optimization_mask=(initial&~GEKKO2_OPT_BIT(GEKKO2_OPT_IOP_DEADLINES))|(fast?GEKKO2_OPT_BIT(GEKKO2_OPT_IOP_DEADLINES):0);
  invoke(&s,IOP_HLE_THREAD_GETTHREADID,0,0,0);
  uint32_t rng=0x13420001;uint64_t h=1469598103934665603ull;
  for(unsigned i=0;i<12000;i++) {
   rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;
   s.instructions_executed++;
   if(g.alarm_in_dispatch)invoke(&s,IOP_HLE_THREAD_ALARM_RETURN_TRAMPOLINE,0,0,0);
   unsigned action=rng%211u;
   if(action==0)invoke(&s,IOP_HLE_THREAD_SETALARM,(rng>>16)%7u,0x2000,0x40);
   if(action==1)invoke(&s,IOP_HLE_THREAD_CANCELALARM,0x2000,0x40,0);
   if(action==2)invoke(&s,IOP_HLE_THREAD_DELAYTHREAD,(rng>>16)%4u,0,0);
   if(action==3)invoke(&s,IOP_HLE_THREAD_RELEASEWAITTHREAD,1,0,0);
   if(action==4)invoke(&s,IOP_HLE_THREAD_SUSPENDTHREAD,1,0,0);
   if(action==5)invoke(&s,IOP_HLE_THREAD_RESUMETHREAD,1,0,0);
   if(i==1000)s.instructions_executed=UINT64_MAX-100;
   iop_hle_thread_tick(&s);
   h=hash(&g,sizeof g,h);h=hash(&s,sizeof s,h);
   if(!fast)signatures[i]=h;else assert(signatures[i]==h);
  }
  if(fast)assert(tick_hint_skips>0&&tick_hint_scans>0);
 }
 /* A retained raw checkpoint pointer can mutate at any later tick. */
 iop_hle_thread_init();memset(&s,0,sizeof s);invoke(&s,IOP_HLE_THREAD_GETTHREADID,0,0,0);
 iop_hle_thread_tick(&s);uint32_t n;void *blob=iop_hle_thread_get_checkpoint_blob(&n);
 assert(blob==&g&&n==sizeof g&&tick_blob_exposed);
 g.alarms[0]=(iop_alarm_t_internal){1,0x4000,0x99,0};
 iop_hle_thread_tick(&s);assert(g.alarm_in_dispatch&&s.pc==0x4000&&s.gpr[4]==0x99);
 gekko2_optimization_mask=initial;
 puts("PASS 12000 exact THREADMAN tick traces, alarms, cancellation, delays, suspension, clock wrap and mutable checkpoint fallback");
}
