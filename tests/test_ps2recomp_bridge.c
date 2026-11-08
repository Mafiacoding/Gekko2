#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/recompiler/ps2recomp_bridge.h"
int main(void)
{
 ee_state_t s;ee_recomp_bridge_t b;uint8_t v8=0;uint16_t v16=0;uint32_t v32=0;uint64_t v64=0;
 ee_reg128_t v128={0,0},w128={0x0123456789abcdefull,0xfedcba9876543210ull};
 memset(&s,0,sizeof(s));memset(&b,0,sizeof(b));s.ram_size=32u*1024u*1024u;
 s.ram=(uint8_t *)calloc(1,s.ram_size);assert(s.ram);
 assert(ee_recomp_bridge_validate(&b)==EE_RECOMP_BRIDGE_BAD_HANDLE);
 assert(ee_recomp_bridge_bind(&b,&s)==EE_RECOMP_BRIDGE_OK);
 assert(ee_recomp_bridge_validate(&b)==EE_RECOMP_BRIDGE_OK);
 assert(ee_recomp_bridge_write8(&b,0x80000104u,0xabu)==EE_RECOMP_BRIDGE_OK);
 assert(ee_recomp_bridge_read8(&b,0x80000104u,&v8)==EE_RECOMP_BRIDGE_OK&&v8==0xabu);
 assert(ee_recomp_bridge_write16(&b,0x80000106u,0xcdefu)==EE_RECOMP_BRIDGE_OK);
 assert(ee_recomp_bridge_read16(&b,0x80000106u,&v16)==EE_RECOMP_BRIDGE_OK&&v16==0xcdefu);
 assert(ee_recomp_bridge_write32(&b,0x80000100u,0x78563412u)==EE_RECOMP_BRIDGE_OK);
 assert(s.ram[0x100]==0x12&&s.ram[0x101]==0x34&&s.ram[0x102]==0x56&&s.ram[0x103]==0x78);
 assert(ee_recomp_bridge_read32(&b,0x80000100u,&v32)==EE_RECOMP_BRIDGE_OK&&v32==0x78563412u);
 assert(ee_recomp_bridge_write64(&b,0x80000108u,0x1122334455667788ull)==EE_RECOMP_BRIDGE_OK);
 assert(ee_recomp_bridge_read64(&b,0x80000108u,&v64)==EE_RECOMP_BRIDGE_OK&&v64==0x1122334455667788ull);
 assert(ee_recomp_bridge_write128(&b,0x80000110u,&w128)==EE_RECOMP_BRIDGE_OK);
 assert(ee_recomp_bridge_read128(&b,0x80000110u,&v128)==EE_RECOMP_BRIDGE_OK);
 assert(v128.ud0==w128.ud0&&v128.ud1==w128.ud1);
 /* A host test's detached EE state must never enter the live scheduler
  * window. Refusal is side-effect free and leaves precise execution state. */
 s.pc=0x1000u;s.next_pc=0x1004u;
 { const uint32_t nops[2]={0u,0u};uint64_t retired=s.instructions_executed;
   assert(ee_recomp_bridge_begin_native_window(&b,s.pc,nops,2u,2u,0u)==0u);
   assert(s.pc==0x1000u&&s.instructions_executed==retired); }
 v32=0xa5a5a5a5u;
 assert(ee_recomp_bridge_read32(&b,0x00001000u,&v32)==EE_RECOMP_BRIDGE_GUEST_FAULT);
 assert(v32==0xa5a5a5a5u);
 b.abi_version++;assert(ee_recomp_bridge_validate(&b)==EE_RECOMP_BRIDGE_BAD_HANDLE);
 free(s.ram);puts("PASS PS2Recomp bridge handle, 8/16/32/64/128-bit memory callbacks, native-window refusal, TLB-fault reporting");return 0;
}
