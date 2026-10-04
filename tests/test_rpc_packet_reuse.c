/* Integration-level descriptor lifecycle from ps2sdk sifrpc.c::_request_end. */
#include <stdio.h>
#include "core/ee/ee_core.c"
static unsigned fails;
#define CHECK(x,msg) do{if(!(x)){printf("FAIL: %s\n",msg);fails++;}}while(0)
int main(void){bios_image_t b={0};if(system_init(&b,&b))return 2;ee_state_t*s=ee_core_get_state();
const uint32_t cd=0xa1fc79c8,pkt=0xa1fc8c00;
for(unsigned i=0;i<64;i++){
 ee_mem_write32(s,cd,pkt);ee_mem_write32(s,cd+4,i+2);ee_mem_write32(s,cd+0x24,0);ee_mem_write32(s,cd+0x28,0x13579bdf);ee_mem_write32(s,cd+0x2c,0x2468ace0);
 ee_mem_write32(s,pkt+8,SIF_CMD_RPC_BIND);ee_mem_write32(s,pkt+0x10,0x150005);ee_mem_write32(s,pkt+0x18,i+2);ee_mem_write32(s,pkt+0x1c,cd);
 ee_hle_rpc_complete_descriptor(s,cd,SIF_CMD_RPC_BIND);
 CHECK(ee_mem_read32(s,cd+0x24)==0x1000,"BIND publishes non-null server before resuming caller");
 CHECK(ee_mem_read32(s,cd+0x28)==0x13579bdf&&ee_mem_read32(s,cd+0x2c)==0x2468ace0,"BIND does not overwrite adjacent client data");
 CHECK(ee_mem_read32(s,pkt+0x10)==0x150004,"packet frees allocation bit and retains index/mode");
 CHECK(ee_mem_read32(s,pkt+0x18)==0&&ee_mem_read32(s,cd)==pkt,"packet id is free while guest handler retains a valid pointer");
}
ee_mem_write32(s,cd,pkt);ee_mem_write32(s,cd+4,19);ee_mem_write32(s,cd+0x24,0x5678);ee_mem_write32(s,pkt+8,SIF_CMD_RPC_CALL);ee_mem_write32(s,pkt+0x10,5);ee_mem_write32(s,pkt+0x18,19);ee_mem_write32(s,pkt+0x1c,cd);
ee_hle_rpc_complete_descriptor(s,cd,SIF_CMD_RPC_CALL);
CHECK(ee_mem_read32(s,cd+0x24)==0x5678,"CALL preserves existing server");CHECK(ee_mem_read32(s,pkt+0x18)==0&&ee_mem_read32(s,cd)==pkt,"CALL frees packet without invalidating pending guest handler");
ee_mem_write32(s,cd,pkt);ee_mem_write32(s,pkt+0x10,5);ee_mem_write32(s,pkt+0x18,20);ee_mem_write32(s,cd+4,21);ee_hle_rpc_complete_descriptor(s,cd,SIF_CMD_RPC_CALL);
CHECK(ee_mem_read32(s,cd)==pkt&&ee_mem_read32(s,pkt+0x10)==5,"stale reply cannot free another outstanding rpc id");
ee_hle_rpc_complete_descriptor(s,0xdeadbeef,SIF_CMD_RPC_BIND);
printf("RPC reuse: %u failures across 64 consecutive binds\n",fails);return fails?1:0;}
