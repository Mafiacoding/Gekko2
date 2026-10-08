#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/ee/ee_core.c"
#include "core/system.h"

static void le32(uint8_t *p, uint32_t v){for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static void le16(uint8_t *p, uint16_t v){p[0]=v;p[1]=v>>8;}
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
    bios_image_t bios={0};bios.data=calloc(1,BIOS_MAX_SIZE);bios.size=BIOS_MAX_SIZE;bios.loaded=1;
    /* Small synthetic ROMDIR and ELF, with no copyrighted ROM payload. */
    memcpy(bios.data+0x200,"RESET",5);le32(bios.data+0x20c,0x200);
    memcpy(bios.data+0x210,"ROMDIR",6);le32(bios.data+0x21c,64);
    memcpy(bios.data+0x220,"XPADMAN",7);le32(bios.data+0x22c,256);
    uint8_t *e=bios.data+0x240;memcpy(e,"\177ELF\1\1",6);
    le32(e+32,52);le16(e+46,40);le16(e+48,2);
    le32(e+96,0x70000080);le32(e+108,132);le32(e+112,26);le16(e+156,0x0306);
    if(system_init(&bios,&bios))return 1;
    ee_state_t *s=ee_core_get_state();
    s->loadfile_rpc_version=0x34333231u;
    call(s,SIF_SID_LOADFILE,255,0,4);
    if(ee_mem_read32(s,0x80006000)!=0x34333231u || ee_mem_read32(s,0x80006004)!=0xcafef00d)return puts("FAIL LOADFILE protocol reply/bounds"),1;
    for(unsigned bytes=0;bytes<4;bytes++){
        call(s,SIF_SID_LOADFILE,255,0,bytes);
        if(ee_mem_read32(s,0x80006000)!=0xfeedface)return puts("FAIL short LOADFILE protocol reply overwritten"),1;
    }
    s->loadfile_rpc_version=0;
    call(s,SIF_SID_LOADFILE,255,0,4);
    if(ee_mem_read32(s,0x80006000))return puts("FAIL unknown LOADFILE protocol invented"),1;
    call(s,0x12,5,0,256);
    if((int32_t)ee_mem_read32(s,0x80006000)!=-3||ee_mem_read32(s,0x80006004)!=0xcafef00d)return puts("FAIL absent XFROM boot image reply"),1;
    call(s,0x12,1,0,4);
    if((int32_t)ee_mem_read32(s,0x80006000)!=-1)return puts("FAIL absent ATA init reply"),1;
    call(s,0x12,2,0,0);
    if(ee_mem_read32(s,0x80006000)!=0xfeedface)return puts("FAIL zero-size response overwritten"),1;
    call(s,SIF_SID_PAD_BIND_ID1_NEW,1,0x12,128);
    if(ee_mem_read32(s,0x8000600c)!=0x0306)return puts("FAIL PAD version uses actual payload/ROM metadata"),1;
    call(s,SIF_SID_CDVD_SCMD,3,0,4);
    if(ee_mem_read32(s,0x80006000)!=IOP_CDVD_TYPE_NODISC)return puts("FAIL diskless RPC falsely reports disc"),1;
    iop_cdvd_set_disc_present(0x14);
    call(s,SIF_SID_CDVD_SCMD,3,0,4);
    if(ee_mem_read32(s,0x80006000)!=0x14)return puts("FAIL RPC/MMIO media type mismatch"),1;
    iop_cdvd_unmount_iso();
    call(s,SIF_SID_CDVD_SCMD,1,0,12);
    if(ee_mem_read32(s,0x80006000)!=1||ee_mem_read8(s,0x80006004)!=0||ee_mem_read8(s,0x80006008)!=0)return puts("FAIL RTC result/status/padding"),1;
    for(unsigned j=1;j<8;j++)if(j!=4){unsigned v=ee_mem_read8(s,0x80006004+j);if((v&15)>9||(v>>4)>9)return puts("FAIL RTC BCD encoding"),1;}
    if(!ee_mem_read8(s,0x80006009)||!ee_mem_read8(s,0x8000600a))return puts("FAIL RTC zero day/month"),1;
    if(ee_mem_read32(s,0x8000600c)!=0xdeadbeef)return puts("FAIL RTC response bounds"),1;
    call(s,SIF_SID_CDVD_SCMD,1,0,0);
    if(ee_mem_read32(s,0x80006000)!=0xfeedface)return puts("FAIL RTC zero-size reply overwrite"),1;
    le32(e+108,0xfffffff0);
    if(ee_rom_module_version(&bios,"XPADMAN")!=0)return puts("FAIL malformed ELF accepted"),1;
    s->pc=0x80007000;s->next_pc=0x80007004;s->cop0[12]=0;s->cop0[13]=0;
    s->branch_pending=0;s->halted=0;s->idle=0;s->gpr[3].ud0=61;
    s->gpr[4].ud0=0x00100000;s->gpr[5].ud0=0xffffffff;s->gpr[2].ud0=0xdeadbeef;
    ee_step();
    if(s->pc!=0x80000180 || (s->cop0[13]&0x7c)!=0x20 || s->cop0[14]!=0x80007000 ||
       s->gpr[4].ud0!=0x00100000 || s->gpr[5].ud0!=0xffffffff || s->gpr[2].ud0!=0xdeadbeef)
        return puts("FAIL SetupHeap must reach real BIOS with original arguments"),1;
    puts("PASS real SifSetDma RPC dispatch: absent boot devices, response bounds, PAD module version");return 0;
}
