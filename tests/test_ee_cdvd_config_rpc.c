#include <stdio.h>
#include <string.h>
#include "core/ee/ee_core.c"
static unsigned fails;
static unsigned transfer_size=~0u;
#define CHECK(c,m) do{if(!(c)){printf("FAIL: %s\n",m);fails++;}}while(0)
static void rpc(ee_state_t*s,unsigned fno,unsigned send,unsigned recv){
 const uint32_t d=0x80002000,h=0x80004000,c=0x80005000,r=0x80006000;
 ee_mem_write32(s,d,0x80003000);ee_mem_write32(s,d+8,transfer_size==~0u?send:transfer_size);ee_mem_write32(s,d+16,h);ee_mem_write32(s,d+24,64);ee_mem_write32(s,h,64);ee_mem_write32(s,h+8,SIF_CMD_RPC_CALL);ee_mem_write32(s,h+0x1c,c);ee_mem_write32(s,h+0x20,fno);ee_mem_write32(s,h+0x24,send);ee_mem_write32(s,h+0x28,r);ee_mem_write32(s,h+0x2c,recv);
 sif_cmd_iop_track_bind_sid(c,SIF_SID_CDVD_SCMD);s->pc=0x80007000;s->next_pc=s->pc+4;ee_mem_write32(s,s->pc,12);s->gpr[3].ud0=119;s->gpr[4].ud0=d;s->gpr[5].ud0=2;s->cop0[12]=0;s->idle=s->halted=0;ee_step();
}
int main(void){bios_image_t b={0};if(system_init(&b,&b))return 2;ee_state_t*s=ee_core_get_state();
 ee_mem_write32(s,0x80003000,1|(1<<8)|(2<<16));rpc(s,0xe,4,8);CHECK(ee_mem_read32(s,0x80006000)==1&&!ee_mem_read32(s,0x80006004),"genuine OpenConfig RPC selects write bank 1/count 2");
 for(unsigned i=0;i<30;i++)ee_mem_write8(s,0x80003000+i,i+1);rpc(s,0x11,0x400,8);CHECK(ee_mem_read32(s,0x80006000)==2,"write returns actual completed blocks");
 /* Read the same data through hardware MMIO, not a separate HLE store. */
 iop_cdvd_mmio_write8(IOP_CDVD_BASE+0x17,0);iop_cdvd_mmio_write8(IOP_CDVD_BASE+0x17,1);iop_cdvd_mmio_write8(IOP_CDVD_BASE+0x17,2);iop_cdvd_mmio_write8(IOP_CDVD_BASE+0x16,0x40);uint8_t v;iop_cdvd_mmio_read8(IOP_CDVD_BASE+0x18,&v);CHECK(!v,"hardware OpenConfig acknowledgement");
 for(unsigned j=0;j<2;j++){iop_cdvd_mmio_write8(IOP_CDVD_BASE+0x16,0x41);unsigned sum=0;for(unsigned i=0;i<16;i++){iop_cdvd_mmio_read8(IOP_CDVD_BASE+0x18,&v);if(i<15){CHECK(v==j*15+i+1,"MMIO reads RPC-written data");sum+=v;}else CHECK(v==(uint8_t)sum,"hardware checksum generated at provider boundary");}}
 ee_mem_write32(s,0x80003000,(1<<8)|(2<<16));rpc(s,0xe,4,8);memset(s->ram+0x6008,0xcd,0x400);rpc(s,0x10,0,0x408);CHECK(ee_mem_read32(s,0x80006000)==2,"read returns completed block count");for(unsigned i=0;i<30;i++)CHECK(ee_mem_read8(s,0x80006008+i)==i+1,"RPC strips checksum bytes");CHECK(ee_mem_read8(s,0x80006026)==0xcd,"read leaves unused reply bytes untouched");
 rpc(s,0xf,0,8);rpc(s,0x10,0,0x408);CHECK(!ee_mem_read32(s,0x80006000)&&ee_mem_read32(s,0x80006004)==0x80,"closed session cannot fabricate successful read");
 ee_mem_write32(s,0x80003000,(1<<8)|(2<<16));rpc(s,0xe,4,8);rpc(s,0x10,0,23);CHECK(ee_mem_read32(s,0x80006000)==1&&ee_mem_read32(s,0x80006004)==0x80,"short reply reports partial count without overflowing");
 ee_mem_write32(s,0x80006000,0xfeedface);rpc(s,0xe,4,4);CHECK(ee_mem_read32(s,0x80006000)==0xfeedface,"truncated header does not overrun caller buffer");
 uint8_t corrupt[16]={7};cdvd_config_open(1,1,1);cdvd_config_write(corrupt);
 ee_mem_write32(s,0x80003000,(1<<8)|(1<<16));rpc(s,0xe,4,8);rpc(s,0x10,0,0x408);
 CHECK(!ee_mem_read32(s,0x80006000)&&ee_mem_read32(s,0x80006004)==1,"checksum error returns actual provider status 1/completed count zero");
 CHECK(ee_mem_read8(s,0x80006008)==7,"provider exposes corrupt block data before checksum rejection");
 ee_mem_write32(s,0x80003000,1|(1<<8)|(2<<16));rpc(s,0xe,4,8);
 transfer_size=15;rpc(s,0x11,0x400,8);transfer_size=~0u;
 CHECK(ee_mem_read32(s,0x80006000)==1&&ee_mem_read32(s,0x80006004)==0x80,"request size cannot exceed actual preceding DMA payload extent");
 printf("CDVD config RPC/MMIO integration: %u failures\n",fails);return fails?1:0;}
