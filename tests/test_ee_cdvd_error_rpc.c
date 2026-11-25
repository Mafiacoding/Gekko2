/* Synthetic packets only: do not turn an error query into success/failure 1. */
#define main config_rpc_fixture_main
#include "test_ee_cdvd_config_rpc.c"
#undef main
int main(void){
 bios_image_t b={0};if(system_init(&b,&b))return 2;
 ee_state_t*s=ee_core_get_state();unsigned checks=0;
 for(unsigned error=0;error<256;error+=17)for(unsigned bytes=0;bytes<=12;bytes++){
  iop_cdvd_checkpoint_t drive;iop_cdvd_checkpoint_save(&drive);
  drive.regs[6]=(uint8_t)error; /* CDVD ERROR register at base+0x06. */
  iop_cdvd_checkpoint_load(&drive);
  /* Each packet is a separate fixture transaction, not an unserviced queue. */
  g_rpc_bind_pending=0;g_rpc_bind_qcount=0;g_rpc_bind_qhead=g_rpc_bind_qtail=0;
  memset(s->ram+0x6000,0xa5,16);rpc(s,4,0,bytes);
  CHECK(ee_mem_read32(s,0x80006000)==(bytes>=4?error:0xa5a5a5a5u),"GetError returns the drive code or preserves a short reply");checks++;
  for(unsigned n=4;n<16;n++){CHECK(ee_mem_read8(s,0x80006000+n)==0xa5,"GetError writes only its single reply word");checks++;}
  CHECK(iop_cdvd_peek_last_ncmd_error()==error,"RPC query does not clear the hardware error");checks++;
 }
 printf("%s %u CDVD GetError RPC error-state, short-reply and sentinel checks (%u failures)\n",fails?"FAIL":"PASS",checks,fails);
 return fails?1:0;
}
