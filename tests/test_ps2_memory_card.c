#include "core/hw/ps2_memory_card.h"
#include "core/hw/iop_sio2.h"
#include "core/hw/iop_dma.h"
#include "core/hw/iop_intc.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void put(uint8_t *p,unsigned v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static uint8_t reply[300];
static void select_page(unsigned cmd,unsigned page)
{
 uint8_t in[9]={0x81,(uint8_t)cmd};put(in+2,page);in[6]=in[2]^in[3]^in[4]^in[5];
 ps2_mc_transfer(in,reply,9);assert(reply[7]==0x2b&&reply[8]==0x55);
}
static void send_data(const uint8_t *data,unsigned bytes)
{
 uint8_t in[300]={0x81,0x42};in[2]=(uint8_t)bytes;
 for(unsigned i=0;i<bytes;i++){in[3+i]=data[i];in[3+bytes]^=data[i];}
 ps2_mc_transfer(in,reply,bytes+6u);assert(reply[3]==0x2b&&reply[bytes+5u]==0x55);
}
static void read_data(unsigned bytes)
{uint8_t in[300]={0x81,0x43};in[2]=(uint8_t)bytes;ps2_mc_transfer(in,reply,bytes+6u);assert(reply[3]==0x2b&&reply[bytes+5u]==0x55);}
int main(void)
{
 char path[128];snprintf(path,sizeof path,"/tmp/gekko2-ps2-card-%ld.ps2",(long)getpid());
 FILE *f=fopen(path,"wb");assert(f);uint8_t page[512];memset(page,255,sizeof page);
 for(unsigned n=0;n<PS2_MC_PAGES;n++)assert(fwrite(page,1,512,f)==512);assert(!fclose(f));
 assert(!ps2_mc_mount(path,0)&&ps2_mc_present());unsigned formatted,free_clusters;
 assert(ps2_mc_get_info(&formatted,&free_clusters)==-2&&!formatted&&!free_clusters);
 uint8_t ecc[3],data[128]={0};ps2_mc_ecc128(data,ecc);assert(ecc[0]==0x77&&ecc[1]==0x7f&&ecc[2]==0x7f);
 const uint8_t column[8]={0x70,0x61,0x52,0x43,0x34,0x25,0x16,0x07};
 for(unsigned i=0;i<128;i++)for(unsigned b=0;b<8;b++){
  memset(data,0,128);data[i]=(uint8_t)(1u<<b);ps2_mc_ecc128(data,ecc);
  assert(ecc[0]==column[b]&&ecc[1]==i&&ecc[2]==127u-i);
 }
 for(unsigned i=0;i<128;i++)data[i]=(uint8_t)(i*13u+7u);
 select_page(0x22,100);send_data(data,128);assert(!ps2_mc_flush());
 select_page(0x23,100);read_data(128);assert(!memcmp(reply+4,data,128));
 uint32_t size;const void *blob=ps2_mc_snapshot(&size);void *saved=malloc(size);assert(saved);memcpy(saved,blob,size);
 assert(ps2_mc_snapshot_valid(saved,size));select_page(0x21,100);uint8_t erase[4]={0x81,0x82};ps2_mc_transfer(erase,reply,4);assert(reply[2]==0x2b);
 select_page(0x23,100);read_data(128);for(unsigned i=0;i<128;i++)assert(reply[i+4]==255);
 assert(!ps2_mc_snapshot_load(saved,size));free(saved);select_page(0x23,100);read_data(128);assert(!memcmp(reply+4,data,128));
 assert(!ps2_mc_eject()&&!ps2_mc_mount(path,0));select_page(0x23,100);read_data(128);assert(!memcmp(reply+4,data,128));
 uint8_t bad[9]={0x81,0x22,0xff,0xff};bad[6]=0;ps2_mc_transfer(bad,reply,9);assert(reply[0]==255);
 uint8_t term[5]={0x81,0x27,0x5a};ps2_mc_transfer(term,reply,5);assert(reply[4]==0x5a);
 term[1]=0xf3;ps2_mc_transfer(term,reply,5);assert(reply[4]==0x55);
 /* Real DMA input/output, two SEND3 frames and 16-byte DMA padding. */
 iop_sio2_init();iop_dma_init();iop_intc_init();uint8_t ram[1024]={0};iop_dma_bind_iop_ram(ram,sizeof ram);
 ram[0x100]=ram[0x110]=0x81;ram[0x101]=0x26;ram[0x111]=0x11;
 iop_sio2_mmio_write32(IOP_SIO2_BASE,(13u<<8)|(13u<<18)|2u);
 iop_sio2_mmio_write32(IOP_SIO2_BASE+4,(4u<<8)|(4u<<18)|2u);
 iop_dma_mmio_write32(0x1f801540,0x100);iop_dma_mmio_write32(0x1f801544,(2u<<16)|4u);iop_dma_mmio_write32(0x1f801548,0x01000201);
 assert(!(iop_dma_get_state()->ch[11].chcr&0x01000000u));
 iop_dma_mmio_write32(0x1f801550,0x200);iop_dma_mmio_write32(0x1f801554,(2u<<16)|4u);iop_dma_mmio_write32(0x1f801558,0x01000200);
 assert(iop_dma_get_state()->ch[12].chcr&0x01000000u);
 iop_sio2_mmio_write8(IOP_SIO2_BASE+0x68,1);
 assert(!(iop_dma_get_state()->ch[12].chcr&0x01000000u));
 assert(ram[0x202]==0x2b&&ram[0x204]==2&&ram[0x208]==0x40&&ram[0x20b]==0x52&&ram[0x20c]==0x55);
 assert(ram[0x212]==0x2b&&ram[0x213]==0x55);
 assert(iop_dma_get_state()->ch[12].madr==0x220&&!iop_sio2_transport_pending());
 assert(!ps2_mc_eject());assert(!ps2_mc_mount(path,1));select_page(0x22,100);
 uint8_t write[8]={0x81,0x42,1,0,0};ps2_mc_transfer(write,reply,7);assert(reply[0]==255);
 assert(!ps2_mc_eject());
 /* Own minimal superblock/FAT metadata: two allocated, two free clusters.
  * This is not a complete formatted guest filesystem or a user image. */
 f=fopen(path,"r+b");assert(f);memset(page,0,512);
 memcpy(page,"Sony PS2 Memory Card Format ",28);page[41]=2;page[42]=2;page[44]=16;
 put(page+48,8192);put(page+52,64);put(page+56,4);put(page+80,1);
 assert(fwrite(page,1,512,f)==512);assert(!fseek(f,1024,SEEK_SET));put(page,2);assert(fwrite(page,1,4,f)==4);
 assert(!fseek(f,2048,SEEK_SET));put(page,0xffffffffu);put(page+4,0x7fffffffu);put(page+8,0x7fffffffu);put(page+12,0x80000002u);
 assert(fwrite(page,1,16,f)==16);assert(!fclose(f));assert(!ps2_mc_mount(path,1));
 assert(!ps2_mc_get_info(&formatted,&free_clusters)&&formatted==1&&free_clusters==2);
 assert(!ps2_mc_eject());remove(path);
 puts("PASS native PS2 card: 1024 ECC vectors, raw mount/persistence, NAND read/write/erase, cursor snapshot, malformed/readonly commands, SEND3 and paused SIO2 DMA completion");
}
