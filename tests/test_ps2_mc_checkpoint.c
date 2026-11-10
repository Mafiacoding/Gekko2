#include "core/system.h"
#include "core/checkpoint.h"
#include "core/hw/ps2_memory_card.h"
#include "core/ee/ee_core.c"
#include <assert.h>
#include <unistd.h>
int main(void)
{
 bios_image_t bios={0};assert(!system_init(&bios,&bios));
 char card[128],snapshot[128];snprintf(card,sizeof card,"/tmp/gekko2-card-ckpt-%ld.ps2",(long)getpid());
 snprintf(snapshot,sizeof snapshot,"/tmp/gekko2-card-ckpt-%ld.bin",(long)getpid());
 FILE *f=fopen(card,"wb");assert(f);uint8_t page[528];memset(page,255,sizeof page);
 for(unsigned n=0;n<PS2_MC_PAGES;n++)assert(fwrite(page,1,528,f)==528);assert(!fclose(f));assert(!ps2_mc_mount(card,0));
 ee_state_t *s=ee_core_get_state();uint32_t request=0xa0110000u,param=0xa0120000u;
 for(unsigned n=0;n<48;n+=4)ee_mem_write32(s,request+n,0);
 ee_mem_write32(s,request+20,1);ee_mem_write32(s,request+28,param);
 assert(ee_mcserv_getinfo_no_card(s,request,48,1)==-2);
 assert(ee_mem_read32(s,param)==2&&ee_mem_read32(s,param+4)==0&&ee_mem_read32(s,param+144)==0);
 assert(!checkpoint_save(snapshot));
 uint8_t command[9]={0x81,0x22,10},out[9];command[6]=10;ps2_mc_transfer(command,out,9);assert(out[7]==0x2b);
 uint8_t write[7]={0x81,0x42,1,0,0};ps2_mc_transfer(write,out,7);assert(out[3]==0x2b);
 assert(!checkpoint_load(snapshot,&bios,&bios,NULL));
 command[1]=0x23;ps2_mc_transfer(command,out,9);uint8_t read[7]={0x81,0x43,1};ps2_mc_transfer(read,out,7);assert(out[4]==255);
 iop_sio2_mmio_write8(IOP_SIO2_BASE+0x60,0x81);assert(checkpoint_save(snapshot)<0);
 iop_sio2_mmio_write8(IOP_SIO2_BASE+0x68,12);assert(!ps2_mc_eject());
 remove(card);remove(snapshot);puts("PASS native card GETINFO type/format DMA, full checkpoint restore and rejection of in-flight serial snapshots");
}
