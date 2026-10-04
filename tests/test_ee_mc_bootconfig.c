/* Genuine reboot DMA -> MCSERV INIT with a synthetic nested ROM config. */
#include <stdio.h>
#include <string.h>
#include "core/ee/ee_core.c"
static unsigned failures;
#define CHECK(c,m) do {if(!(c)){printf("FAIL: %s\n",m);failures++;}}while(0)
static void p32(uint8_t*p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=n>>(8*i);}
static void entry(uint8_t*p,const char*n,uint32_t size){memcpy(p,n,strlen(n));p32(p+12,size);}
static void irx(uint8_t*p,const char*n,unsigned v){memcpy(p,"\177ELF",4);p[4]=p[5]=1;p32(p+32,52);p[46]=40;p[48]=1;p32(p+56,0x70000080);p32(p+68,100);p32(p+72,60);p[124]=v;p[125]=v>>8;strcpy((char*)p+126,n);}
static void dma(ee_state_t*s,unsigned n){s->pc=0x80007000;s->next_pc=s->pc+4;ee_mem_write32(s,s->pc,12);s->gpr[3].ud0=119;s->gpr[4].ud0=0x80002000;s->gpr[5].ud0=n;s->cop0[12]=0;s->idle=s->halted=0;ee_step();}
static void reset(ee_state_t*s,const char*args,unsigned len,unsigned bytes,unsigned mode){
 memset(s->ram+0x2000,0,0x5000);ee_mem_write32(s,0x80002000,0x80003000);ee_mem_write32(s,0x80002004,0x10000);ee_mem_write32(s,0x80002008,bytes);
 ee_mem_write32(s,0x80003000,104);ee_mem_write32(s,0x80003008,0x80000003);ee_mem_write32(s,0x80003010,len);ee_mem_write32(s,0x80003014,mode);memcpy(s->ram+0x3018,args,strlen(args)+1);dma(s,1);
}
static void init(ee_state_t*s){
 const uint32_t d=0x80002000,h=0x80004000,c=0x80005000,r=0x80006000;
 memset(s->ram+0x2000,0,0x5000);ee_mem_write32(s,d,0x80003000);ee_mem_write32(s,d+8,48);ee_mem_write32(s,d+16,h);ee_mem_write32(s,d+24,64);ee_mem_write32(s,h,64);ee_mem_write32(s,h+8,SIF_CMD_RPC_CALL);ee_mem_write32(s,h+0x1c,c);ee_mem_write32(s,h+0x20,0xfe);ee_mem_write32(s,h+0x28,r);ee_mem_write32(s,h+0x2c,12);ee_mem_write32(s,r+12,0xfeedface);sif_cmd_iop_track_bind_sid(c,SIF_SID_MCSERV);dma(s,2);
 CHECK(ee_mem_read32(s,r)==0,"INIT success result");CHECK(ee_mem_read32(s,r+4)==0x208,"INIT selected MCSERV version");CHECK(ee_mem_read32(s,r+8)==0x209,"INIT selected mcman_cex version");CHECK(ee_mem_read32(s,r+12)==0xfeedface,"INIT reply stays within 12 bytes");
}
int main(void){
 uint8_t rom[816]={0};entry(rom+128,"RESET",128);entry(rom+144,"ROMDIR",112);entry(rom+160,"OSDCNF",96);entry(rom+176,"XMCSERV",160);entry(rom+192,"XMCMAN",160);entry(rom+208,"LEGACY",160);
 uint8_t*cfg=rom+240;entry(cfg,"RESET",0);entry(cfg+16,"ROMDIR",64);entry(cfg+32,"IOPBTCONF",32);memcpy(cfg+64,"@800\nXMCMAN\nXMCSERV\n",20);
 irx(rom+336,"mcserv",0x208);irx(rom+496,"mcman_cex",0x209);irx(rom+656,"mcman",0xfff);
 bios_image_t b={0};b.data=rom;b.size=sizeof rom;if(system_init(&b,&b))return 2;ee_state_t*s=ee_core_get_state();const char*a="rom0:UDNL rom0:OSDCNF";
 reset(s,a,strlen(a),104,0);CHECK(s->mcserv_module_version==0x208&&s->mcman_module_version==0x209,"reboot selects actual config modules, not highest ROM versions");init(s);
 reset(s,a,strlen(a),104,0);ee_mem_write32(s,0x80003000,64);sif_iop_reset_note_mc_config(s,0x80003000,104);CHECK(s->mcman_module_version==0x209,"non-reset RPC payload header cannot invalidate providers");
 reset(s,a,81,104,0);CHECK(s->mcman_module_version==0x209,"oversized arg rejected without clearing valid providers");
 reset(s,a,strlen(a),40,0);CHECK(s->mcman_module_version==0x209,"truncated packet rejected before reading args");
 reset(s,a,strlen(a)+1,104,0);CHECK(s->mcman_module_version==0x209,"embedded terminator is inconsistent with arglen");
 reset(s,"rom0:UDNL rom0:MISSING",21,104,0);CHECK(!s->mcserv_module_version&&!s->mcman_module_version,"missing config cannot inherit old provider versions");
 reset(s,a,strlen(a),104,0);reset(s,a,strlen(a),104,1);CHECK(!s->mcserv_module_version&&!s->mcman_module_version,"unsupported reboot mode cannot invent provider versions");
 reset(s,a,strlen(a),104,0);p32(cfg+44,0xffffffff);reset(s,a,strlen(a),104,0);CHECK(!s->mcserv_module_version&&!s->mcman_module_version,"nested ROM bounds checked before reading module list");
 printf("MCSERV boot config: %u failures\n",failures);return failures?1:0;
}
