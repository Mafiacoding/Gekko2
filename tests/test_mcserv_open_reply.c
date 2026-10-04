/* Both libmc OPEN ABIs must report a missing file, without overrunning replies. */
#include <stdio.h>
#include <string.h>
#include "core/ee/ee_core.c"
static void call(ee_state_t *s, uint32_t sid, uint32_t fno, uint32_t command, uint32_t recvsize){
    const uint32_t d=0x80002000,p=0x80003000,h=0x80004000,c=0x80005000,r=0x80006000;
    memset(s->ram+0x2000,0,0x5000);
    ee_mem_write32(s,d,p);ee_mem_write32(s,d+4,0x10000);ee_mem_write32(s,d+8,128);
    ee_mem_write32(s,d+16,h);ee_mem_write32(s,d+20,0x11000);ee_mem_write32(s,d+24,64);
    ee_mem_write32(s,p,command);
    ee_mem_write32(s,h,64);ee_mem_write32(s,h+8,SIF_CMD_RPC_CALL);
    ee_mem_write32(s,h+0x1c,c);ee_mem_write32(s,h+0x20,fno);
    ee_mem_write32(s,h+0x28,r);ee_mem_write32(s,h+0x2c,recvsize);
    ee_mem_write32(s,r,0xfeedface);ee_mem_write32(s,r+4,0xcafef00d);ee_mem_write32(s,r+12,0xdeadbeef);
    sif_cmd_iop_track_bind_sid(c,sid);
    ee_mem_write32(s,0x80007000,0x0000000c);
    s->pc=0x80007000;s->next_pc=0x80007004;s->gpr[3].ud0=119;s->gpr[4].ud0=d;s->gpr[5].ud0=2;
    s->cop0[12]=0;s->idle=0;s->halted=0;
    ee_step();
}
int main(void){
 bios_image_t bios={0};if(system_init(&bios,&bios))return 2;
 ee_state_t*s=ee_core_get_state();unsigned failures=0;
 for(unsigned i=0;i<2;i++){
  uint32_t fno=i?0x71u:2u;
  call(s,SIF_SID_MCSERV,fno,0,4);
  if((int32_t)ee_mem_read32(s,0x80006000)!=-4){puts("FAIL: OPEN invented a valid descriptor for absent file");failures++;}
  if(ee_mem_read32(s,0x80006004)!=0xcafef00d||ee_mem_read32(s,0x8000600c)!=0xdeadbeef){puts("FAIL: OPEN reply overruns caller buffer");failures++;}
  call(s,SIF_SID_MCSERV,fno,0,0);
  if(ee_mem_read32(s,0x80006000)!=0xfeedface){puts("FAIL: zero-length OPEN reply written");failures++;}
 }
 call(s,SIF_SID_MCSERV,3,0,4);
 if(ee_mem_read32(s,0x80006000)!=0){puts("FAIL: modern CLOSE mistaken for OPEN");failures++;}
 printf("MCSERV OPEN dispatch: %u failures across both ABIs and response bounds\n",failures);return failures?1:0;
}
