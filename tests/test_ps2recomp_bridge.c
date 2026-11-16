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

 /* Versioned read-only integer-leaf snapshot preserves architectural lanes. */
 {
  ee_recomp_ee_state_snapshot_t snap;memset(&snap,0,sizeof(snap));
  s.pc=0x80001000u;s.next_pc=0x80001004u;s.sa_reg=0x12345678u;
  s.gpr[0].ud0=s.gpr[0].ud1=0;
  for(unsigned i=1;i<32;i++){
   s.gpr[i].ud0=0x0123456789abcdefull^(uint64_t)i;
   s.gpr[i].ud1=0xfedcba9876543210ull^(uint64_t)i;
  }
  s.hi.ud0=0x1122334455667788ull;s.hi.ud1=0x99aabbccddeeff00ull;
  s.lo.ud0=0x8877665544332211ull;s.lo.ud1=0x00ffeeddccbbaa99ull;
  assert(ee_recomp_bridge_snapshot_integer_leaf(&b,&snap)==EE_RECOMP_BRIDGE_OK);
  assert(snap.abi_version==EE_RECOMP_EE_STATE_ABI_VERSION);
  assert(snap.struct_size==sizeof(snap)&&snap.profile==EE_RECOMP_EE_STATE_PROFILE_INTEGER_LEAF);
  assert(snap.pc==s.pc&&snap.next_pc==s.next_pc&&snap.sa_reg==s.sa_reg);
  assert(!snap.flags&&!snap.gpr[0].ud0&&!snap.gpr[0].ud1);
  for(unsigned i=1;i<32;i++)assert(snap.gpr[i].ud0==s.gpr[i].ud0&&snap.gpr[i].ud1==s.gpr[i].ud1);
  assert(snap.hi.ud0==s.hi.ud0&&snap.hi.ud1==s.hi.ud1);
  assert(snap.lo.ud0==s.lo.ud0&&snap.lo.ud1==s.lo.ud1);
  ee_recomp_ee_state_snapshot_t before=snap;
  s.branch_pending=1;
  assert(ee_recomp_bridge_snapshot_integer_leaf(&b,&snap)==EE_RECOMP_BRIDGE_UNSUPPORTED_STATE);
  assert(!memcmp(&snap,&before,sizeof(snap)));
  s.branch_pending=0;s.next_pc=s.pc+8u;
  assert(ee_recomp_bridge_snapshot_integer_leaf(&b,&snap)==EE_RECOMP_BRIDGE_UNSUPPORTED_STATE);
  s.next_pc=s.pc+4u;s.gpr[0].ud1=1;
  assert(ee_recomp_bridge_snapshot_integer_leaf(&b,&snap)==EE_RECOMP_BRIDGE_UNSUPPORTED_STATE);
  s.gpr[0].ud1=0;s.exc_in_delay_slot=1;
  assert(ee_recomp_bridge_snapshot_integer_leaf(&b,&snap)==EE_RECOMP_BRIDGE_UNSUPPORTED_STATE);
  s.exc_in_delay_slot=0;s.cop0[12]=2;
  assert(ee_recomp_bridge_snapshot_integer_leaf(&b,&snap)==EE_RECOMP_BRIDGE_UNSUPPORTED_STATE);
  s.cop0[12]=0;
 }

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
