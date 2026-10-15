#include "core/iop/iop_core.c"
#include "core/recompiler/optimization.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void call_a0(iop_state_t *s,uint32_t fn,uint32_t dst,uint32_t src,uint32_t len)
{s->pc=0xa0;s->next_pc=0xa4;s->gpr[9]=fn;s->gpr[4]=dst;s->gpr[5]=src;s->gpr[6]=len;s->gpr[31]=0x1234;assert(iop_hle_bios_try_handle(s,0xa0));}
int main(void)
{
 bios_image_t b={0};assert(iop_core_init(&b)==0);iop_state_t *s=iop_core_get_state();
 uint32_t initial=gekko2_optimization_mask;uint8_t results[2][256];
 for(unsigned enabled=0;enabled<2;enabled++){
  gekko2_optimization_mask=(initial&~GEKKO2_OPT_BIT(GEKKO2_OPT_HLE_RAM))|(enabled?GEKKO2_OPT_BIT(GEKKO2_OPT_HLE_RAM):0);
  for(unsigned i=0;i<256;i++)s->ram[0x4000+i]=(uint8_t)i;
  call_a0(s,IOP_HLE_A0_MEMCPY,0x80004080,0xa0004000,64);
  /* Preserve overlapping forward-copy semantics across different aliases. */
  call_a0(s,IOP_HLE_A0_MEMMOVE,0x80004001,0x4000,31);
  call_a0(s,IOP_HLE_A0_MEMSET,0x40c0,0x5a,32);
  memcpy(results[enabled],s->ram+0x4000,256);
  s->cop0[12]|=0x10000;call_a0(s,IOP_HLE_A0_MEMSET,0x4000,0x11,128);
  assert(!memcmp(results[enabled],s->ram+0x4000,256));s->cop0[12]&=~0x10000;
 }
 assert(!memcmp(results[0],results[1],256));assert(iop_hle_bios_route_stat(0)==95&&iop_hle_bios_route_stat(1)==32);
 gekko2_optimization_mask|=GEKKO2_OPT_BIT(GEKKO2_OPT_STRICT_HLE);
 s->pc=0xa0;s->next_pc=0xa4;s->gpr[9]=0xffff;s->gpr[2]=0xfeed;s->branch_delay_pending=1;
 iop_state_t before=*s;assert(!iop_hle_bios_try_handle(s,0xa0));assert(!memcmp(&before,s,sizeof before));
 assert(iop_hle_bios_route_stat(3)==1);
 call_a0(s,IOP_HLE_A0_ABS,0x80000000,0,0);assert(s->gpr[2]==0x80000000);
 gekko2_optimization_mask=initial;puts("PASS HLE RAM/helper parity, alias overlap, isolate-cache stores, strict untouched guest fallback and INT_MIN");return 0;
}
