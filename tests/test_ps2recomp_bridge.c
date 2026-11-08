#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/recompiler/ps2recomp_bridge.h"
int main(void)
{
 ee_state_t s;ee_recomp_bridge_t b;uint32_t v32=0;uint64_t v64=0;
 ee_reg128_t v128={0,0},w128={0x0123456789abcdefull,0xfedcba9876543210ull};
 memset(&s,0,sizeof(s));memset(&b,0,sizeof(b));s.ram_size=32u*1024u*1024u;
 s.ram=(uint8_t *)calloc(1,s.ram_size);assert(s.ram);
 assert(ee_recomp_bridge_validate(&b)==EE_RECOMP_BRIDGE_BAD_HANDLE);
 assert(ee_recomp_bridge_bind(&b,&s)==EE_RECOMP_BRIDGE_OK);
 assert(ee_recomp_bridge_validate(&b)==EE_RECOMP_BRIDGE_OK);
 assert(ee_recomp_bridge_write32(&b,0x80000100u,0x78563412u)==EE_RECOMP_BRIDGE_OK);
 assert(s.ram[0x100]==0x12&&s.ram[0x101]==0x34&&s.ram[0x102]==0x56&&s.ram[0x103]==0x78);
 assert(ee_recomp_bridge_read32(&b,0x80000100u,&v32)==EE_RECOMP_BRIDGE_OK&&v32==0x78563412u);
 assert(ee_recomp_bridge_write64(&b,0x80000108u,0x1122334455667788ull)==EE_RECOMP_BRIDGE_OK);
 assert(ee_recomp_bridge_read64(&b,0x80000108u,&v64)==EE_RECOMP_BRIDGE_OK&&v64==0x1122334455667788ull);
 assert(ee_recomp_bridge_write128(&b,0x80000110u,&w128)==EE_RECOMP_BRIDGE_OK);
 assert(ee_recomp_bridge_read128(&b,0x80000110u,&v128)==EE_RECOMP_BRIDGE_OK);
 assert(v128.ud0==w128.ud0&&v128.ud1==w128.ud1);
 v32=0xa5a5a5a5u;
 assert(ee_recomp_bridge_read32(&b,0x00001000u,&v32)==EE_RECOMP_BRIDGE_GUEST_FAULT);
 assert(v32==0xa5a5a5a5u);
 b.abi_version++;assert(ee_recomp_bridge_validate(&b)==EE_RECOMP_BRIDGE_BAD_HANDLE);
 free(s.ram);puts("PASS PS2Recomp bridge handle, LE memory callbacks, 128-bit access, TLB-fault reporting");return 0;
}
