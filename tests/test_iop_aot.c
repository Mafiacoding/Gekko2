/* GPL-3.0-or-later. Preserve complete IOP pipeline state and tick grants. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/iop/iop_core.h"
#include "core/hw/iop_intc.h"
#include "core/hw/iop_timers.h"
#include "core/hw/iop_hle_thread.h"
#include "core/hw/iop_asyncio.h"
#include "core/recompiler/optimization.h"
#include "iop_aot_test_fixture.h"
static uint8_t ram[0x200000];
static unsigned callbacks;
static void before_tick(void){callbacks++;}
static void put(uint32_t at,uint32_t w){for(unsigned i=0;i<4;i++)ram[at+i]=(uint8_t)(w>>(8*i));}
static void reset(void){iop_intc_init();iop_timers_init();iop_hle_thread_init();iop_asyncio_init();}
int main(void)
{
 iop_state_t *st=iop_core_get_state();unsigned comparisons=0;
 for(unsigned i=0;i<iop_aot_fixture_count;i++)for(unsigned mode=0;mode<5;mode++){
  uint32_t pc=iop_aot_fixtures[i][0],word=iop_aot_fixtures[i][1];
  memset(st,0,sizeof(*st));memset(ram,0,sizeof ram);st->ram=ram;st->ram_size=sizeof ram;st->pc=pc;st->next_pc=pc+4;st->sched_ticks=1000;
  for(unsigned r=1;r<32;r++)st->gpr[r]=r*0x1234567u;
  st->gpr[1]=0x80180000u;st->gpr[2]=mode==1?0:mode==2?0xffffffffu:0x80000000u;
  if(mode==3){st->load_delay_reg=2;st->load_delay_value=0x12345678u;}
  if(mode==4){st->branch_delay_pending=1;st->branch_pc=pc-4;st->next_pc=pc+8;}
  put(pc&0x1fffffu,word);put((pc&0x1fffffu)+4,0);put((pc&0x1fffffu)+8,0);put(0x180000,0x34567812u);
  iop_state_t initial=*st,reference;
  reset();callbacks=0;gekko2_optimization_mask&=~GEKKO2_OPT_BIT(GEKKO2_OPT_IOP_JIT);
  assert(iop_core_step_interleaved_n(2,before_tick)==2);reference=*st;unsigned expected_callbacks=callbacks;uint8_t data[32];memcpy(data,ram+0x180000,32);
  *st=initial;put(0x180000,0x34567812u);reset();callbacks=0;gekko2_optimization_mask|=GEKKO2_OPT_BIT(GEKKO2_OPT_IOP_JIT);
  assert(iop_core_step_interleaved_n(2,before_tick)==2);
  if(memcmp(st,&reference,sizeof(*st))||memcmp(data,ram+0x180000,32)||callbacks!=expected_callbacks){fprintf(stderr,"IOP AOT mismatch word=%08x mode=%u callbacks=%u/%u\n",word,mode,callbacks,expected_callbacks);return 1;}
  comparisons++;
 }
 assert(iop_aot_executed_count()>0);
 printf("PASS IOP AOT: %u comparisons with pending loads, BD context and exact EE callbacks\n",comparisons);return 0;
}
