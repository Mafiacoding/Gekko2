#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/hw/vif.h"
#include "core/hw/vu.h"
#include "core/hw/gif.h"
static void put(uint8_t*p,unsigned v){for(unsigned i=0;i<4;i++)p[i]=v>>(8*i);}
int main(void){unsigned cmds[]={0x10,0x11,0x13,0x14,0x15,0x17,0x4a};unsigned checks=0;
 for(unsigned i=0;i<7;i++){
  vif_init();vu1_init();gif_init();vu1_state_t*v=vu1_get_state();vif_state_t*f=vif1_get_state();
  put(v->micro,0x800003bf);put(v->micro+4,0x400002ff);put(v->micro+8,0x800003bf);put(v->micro+12,0x2ff);
  v->running=1;v->tpc=0;uint8_t stream[32]={0};put(stream,cmds[i]<<24|(cmds[i]==0x4a?1u<<16:0));
  unsigned mark=4;if(cmds[i]==0x4a){put(stream+4,0x800003bf);put(stream+8,0x400002ff);mark=12;}
  put(stream+mark,0x07001234);
  assert(vif_accept_quadwords(1,stream,2)==2);assert(f->mark==0);assert(f->codes_processed==0);assert(v->tpc==0);assert(f->stall_words==8);
  for(unsigned n=0;n<20;n++)vif_service();assert(f->codes_processed==0&&f->mark==0);
  vu1_tick();vu1_tick();assert(!v->running);vif_service();assert(f->mark==0x1234);assert(!f->stall_words);assert(f->mscal_calls==(cmds[i]==0x14||cmds[i]==0x15));assert(f->mscnt_calls==(cmds[i]==0x17));checks++;
 }
 for(unsigned cmd=0x10;cmd<=0x15;cmd++)if(cmd==0x10||cmd==0x11||cmd==0x13||cmd==0x15){
  vif_init();vu1_init();gif_init();gif_state_t*g=gif_get_state();vif_state_t*f=vif1_get_state();g->arb_owner=1;uint8_t stream[16]={0};put(stream,cmd<<24);put(stream+4,0x07005678);
  vif_accept_quadwords(1,stream,1);if(cmd==0x10)assert(f->mark==0x5678);else{assert(f->mark==0&&f->stall_words);g->arb_owner=0;vif_service();assert(f->mark==0x5678);}checks++;
 }
 printf("PASS %u VIF micro/GIF barrier retention and exactly-once replay cases\n",checks);
}
