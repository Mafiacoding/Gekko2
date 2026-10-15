#include "core/recompiler/iop_lightrec.h"
#include "core/recompiler/optimization.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
uint32_t gekko2_optimization_mask=GEKKO2_OPT_DEFAULT;
static uint32_t random_word(uint32_t *s){*s^=*s<<13;*s^=*s>>17;*s^=*s<<5;return *s;}
static void expected(unsigned fn,uint32_t a,uint32_t b,uint32_t *lo,uint32_t *hi)
{
 if(fn<26){uint64_t p=fn==24?(uint64_t)((int64_t)(int32_t)a*(int32_t)b):(uint64_t)a*b;*lo=(uint32_t)p;*hi=p>>32;return;}
 if(!b){*lo=fn==26&&((int32_t)a)<0?1u:UINT32_MAX;*hi=a;return;}
 if(fn==26){if(a==0x80000000u&&b==UINT32_MAX){*lo=a;*hi=0;}else{*lo=(uint32_t)((int32_t)a/(int32_t)b);*hi=(uint32_t)((int32_t)a%(int32_t)b);}}
 else {*lo=a/b;*hi=a%b;}
}
int main(void)
{
 assert(iop_lightrec_prepare());assert(iop_lightrec_stat(0)==1 && iop_lightrec_stat(1)==0 && iop_lightrec_stat(5)==0);
 iop_state_t state={0};uint32_t seed=0x81423;unsigned runs=0;
 for(unsigned n=0;n<4096;n++)for(unsigned fn=24;fn<=27;fn++){
  for(unsigned r=0;r<32;r++)state.gpr[r]=random_word(&seed);state.gpr[0]=0;
  uint32_t a=random_word(&seed),b=random_word(&seed);
  if(n%32==0)b=0;if(n%32==1){a=0x80000000u;b=UINT32_MAX;}
  unsigned rs=1+n%31,rt=1+(n/31)%31;if(rs==rt)b=a;state.gpr[rs]=a;state.gpr[rt]=b;state.pc=0x80102348;state.next_pc=state.pc+4;
  state.sched_ticks=73;state.instructions_executed=91;state.load_delay_reg=7;state.load_delay_value=99;
  iop_state_t before=state;uint32_t lo,hi;expected(fn,a,b,&lo,&hi);
  uint32_t word=(rs<<21)|(rt<<16)|fn;
  assert(iop_lightrec_try_execute(&state,word));assert(state.lo==lo&&state.hi==hi);
  before.lo=lo;before.hi=hi;assert(!memcmp(&state,&before,sizeof state));runs++;
 }
 assert(iop_lightrec_stat(0)==1&&iop_lightrec_stat(1)==runs&&!iop_lightrec_stat(3));
 assert(!iop_lightrec_try_execute(&state,0x8c220000u)); /* LW never enters synthetic RAM */
 gekko2_optimization_mask&=~GEKKO2_OPT_BIT(GEKKO2_OPT_IOP_JIT);
 assert(!iop_lightrec_try_execute(&state,(7u<<21)|(13u<<16)|24));
 iop_lightrec_reset();assert(!iop_lightrec_stat(0)&&!iop_lightrec_stat(4));
 puts("PASS 16384 actual Lightrec native executions: signed/unsigned MUL/DIV, zero/overflow, state isolation and reset");
}
