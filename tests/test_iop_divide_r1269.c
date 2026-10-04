#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "core/iop/iop_core.c"
static uint8_t test_ram[65536];
int main(void){
 struct {uint32_t fn,a,b,lo,hi;} cases[]={
  {0x1a,1,0,0xffffffffu,1},{0x1a,0xffffffffu,0,1,0xffffffffu},
  {0x1a,0x80000000u,0xffffffffu,0x80000000u,0},
  {0x1a,0xfffffff9u,3,0xfffffffeu,0xffffffffu},
  {0x1b,0x80000000u,0,0xffffffffu,0x80000000u},
  {0x1b,0xffffffffu,3,0x55555555u,0}};
 iop_state_t *s=iop_core_get_state();iop_intc_init();iop_timers_init();int errors=0;
 for(unsigned n=0;n<sizeof cases/sizeof cases[0];n++) {
  memset(s,0,sizeof *s);memset(test_ram,0,sizeof test_ram);s->ram=test_ram;s->ram_size=sizeof test_ram;s->pc=0x80004000;s->next_pc=s->pc+4;
  s->gpr[1]=cases[n].a;s->gpr[2]=cases[n].b;s->hi=0x11223344;s->lo=0x55667788;
  uint32_t iw=(1u<<21)|(2u<<16)|cases[n].fn;
  for(unsigned b=0;b<4;b++)test_ram[0x4000+b]=(uint8_t)(iw>>(b*8));
  iop_core_step();
  if(s->lo!=cases[n].lo||s->hi!=cases[n].hi||s->halted||s->instructions_executed!=1||s->pc!=0x80004004){printf("FAIL: divide case %u hi=%x lo=%x\n",n,s->hi,s->lo);errors++;}
 }
 printf("%s native IOP divide zero, signed overflow, signed remainder and unsigned paths\n",errors?"FAIL":"PASS");return errors!=0;
}
