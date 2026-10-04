/* Public synthetic sectors exercise the verified profile's transport only.
 * The positive module fingerprint is tested by the private cold-boot run. */
#include <stdio.h>
#include <unistd.h>
#include "core/ee/ee_core.c"
static unsigned fails;static uint8_t iop[2*1024*1024];
#define CHECK(c,m) do{if(!(c)){printf("FAIL: %s\n",m);fails++;}}while(0)
static void p32(uint8_t*p,unsigned v){for(int k=0;k<4;k++)p[k]=v>>(k*8);}
static void bridge(void*ctx,uint32_t a,uint8_t v){((uint8_t*)ctx)[a]=v;}
static void fixture(const char*path){
 uint8_t b[24*2048]={0};uint8_t*p=b+16*2048;p[0]=1;memcpy(p+1,"CD001",5);p[6]=1;
 uint8_t*r=p+156;r[0]=34;p32(r+2,20);p32(r+10,2048);r[25]=2;r[32]=1;
 r=b+20*2048;const char*n="TEKKEN.BIN;1";r[0]=33+strlen(n);p32(r+2,21);p32(r+10,6144);r[32]=strlen(n);memcpy(r+33,n,strlen(n));
 for(unsigned k=0;k<6144;k++)b[21*2048+k]=(k*37+19)&255;
 FILE*f=fopen(path,"wb");fwrite(b,1,sizeof(b),f);fclose(f);
}
static void rpc(ee_state_t*s,unsigned command,unsigned sector,unsigned dst,unsigned bytes,unsigned extent){
 unsigned d=0x80002000,h=0x80004000,c=0x80005000,r=0x80006000,p=0x80003000;
 ee_mem_write32(s,p,p);ee_mem_write32(s,p+4,sector);ee_mem_write32(s,p+8,dst);ee_mem_write32(s,p+12,bytes);ee_mem_write32(s,p+16,0);
 ee_mem_write32(s,d,p);ee_mem_write32(s,d+8,extent);ee_mem_write32(s,d+16,h);ee_mem_write32(s,d+24,64);
 ee_mem_write32(s,h,64);ee_mem_write32(s,h+8,SIF_CMD_RPC_CALL);ee_mem_write32(s,h+0x1c,c);ee_mem_write32(s,h+0x20,command);ee_mem_write32(s,h+0x24,64);ee_mem_write32(s,h+0x28,r);ee_mem_write32(s,h+0x2c,4);
 sif_cmd_iop_track_bind_sid(c,SIF_SID_SPU2DRV);s->pc=0x80007000;s->next_pc=s->pc+4;ee_mem_write32(s,s->pc,12);s->gpr[3].ud0=119;s->gpr[4].ud0=d;s->gpr[5].ud0=2;s->cop0[12]=0;s->idle=s->halted=0;ee_step();
}
int main(void){
 bios_image_t b={0};if(system_init(&b,&b))return 2;ee_state_t*s=ee_core_get_state();char path[]="/tmp/rspu2-XXXXXX";int fd=mkstemp(path);close(fd);fixture(path);CHECK(!iop_cdvd_mount_iso(path),"mount synthetic sectors");
 CHECK(!ee_rspu2_disc_profile(),"ordinary disc without matching module cannot enable private opcode layout");
 g_rspu2_disc_profile=1;memset(s->ram+0x10000,0xcd,5000);
 rpc(s,0x204e,0x4821,0x20010000,0x350a+2301,64);
 CHECK(!ee_mem_read32(s,0x80006000),"EE RPC result follows full data transfer");
 for(unsigned k=0;k<2301;k++)CHECK(s->ram[0x10000+k]==(uint8_t)((k+2048)*37+19),"EE sector crossing and final byte match source");
 CHECK(s->ram[0x10000+2301]==0xcd,"EE byte count does not overwrite following destination");
 CHECK(!(s->cop0[13]&0x7c),"uncached SIF bus destination does not cause guest TLB exception");
 ee_core_set_iop_write8_bridge(iop,bridge);memset(iop+0x10000,0xcd,256);
 uint8_t *sound=spu2_mixer_get_ram();memset(sound+0x10000,0xcd,256);
 rpc(s,0x2045,0x4820,0x10000,0x350a+65,64);
 for(unsigned k=0;k<128;k++)CHECK(sound[0x10000+k]==(uint8_t)(k*37+19),"SPU2 transfer applies real 64-byte rounding");
 CHECK(sound[0x10080]==0xcd,"SPU2 rounded boundary preserved");
 for(unsigned k=0;k<256;k++)CHECK(iop[0x10000+k]==0xcd,"SPU2 transfer preserves IOP executable memory");
 rpc(s,0x204e,0x4820,0x20010000,0x350a+100,16);CHECK(ee_mem_read32(s,0x80006000)==~0u,"truncated DMA payload cannot fabricate success");
 rpc(s,0x204e,0x4820,0x21fffff0,0x350a+100,64);CHECK(ee_mem_read32(s,0x80006000)==~0u,"EE destination bounds checked");
 rpc(s,0x2045,0x4820,0x1ffff0,0x350a+100,64);CHECK(ee_mem_read32(s,0x80006000)==~0u,"SPU2 rounded destination bounds checked");
 rpc(s,0x204e,0x4823,0x20010000,0x350a+100,64);CHECK(ee_mem_read32(s,0x80006000)==~0u,"file bounds checked");
 rpc(s,0x204e,0x481f,0x20010000,0x350a+100,64);CHECK(ee_mem_read32(s,0x80006000)==~0u,"encoded sector underflow rejected");
 rpc(s,0x204e,0x4820,0x20010000,0x3509,64);CHECK(ee_mem_read32(s,0x80006000)==~0u,"encoded byte count underflow rejected");
 unlink(path);printf("RSPU2 disc RPC: %u failures\n",fails);return !!fails;
}
