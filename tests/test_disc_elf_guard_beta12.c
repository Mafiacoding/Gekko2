/* Standalone public synthetic disc / EE LOADFILE tests. No BIOS/game payload. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/ee/ee_core.c"
#include "core/system.h"
static void u16(uint8_t *p,uint32_t n){p[0]=n;p[1]=n>>8;}
static void u32(uint8_t *p,uint32_t n){p[0]=n;p[1]=n>>8;p[2]=n>>16;p[3]=n>>24;}
static void rec(uint8_t *p,uint32_t lba,uint32_t size,uint8_t flags,const char *name){
 size_t n=strlen(name);p[0]=(uint8_t)(33+n+(n%2==0));u32(p+2,lba);u32(p+10,size);
 p[25]=flags;p[32]=(uint8_t)n;memcpy(p+33,name,n);
}
static int write_image(const char *path,unsigned kind){
 FILE *f=fopen(path,"wb");if(!f)return -1;
 uint8_t b[2048]={0};for(unsigned i=0;i<16;i++)fwrite(b,1,2048,f);
 b[0]=1;memcpy(b+1,"CD001",5);b[6]=1;b[156]=34;u32(b+158,20);u32(b+166,2048);b[156+25]=2;b[156+32]=1;fwrite(b,1,2048,f);
 memset(b,0,2048);b[0]=255;memcpy(b+1,"CD001",5);b[6]=1;fwrite(b,1,2048,f);
 memset(b,0,2048);fwrite(b,1,2048,f);fwrite(b,1,2048,f);
 memset(b,0,2048);b[0]=34;u32(b+2,20);u32(b+10,2048);b[25]=2;b[32]=1;
 rec(b+34,21,2048,0,"TRUNC.ELF;1");fwrite(b,1,2048,f);
 memset(b,0,2048);memcpy(b,"\177ELF",4);b[4]=1;b[5]=1;u16(b+18,8);
 u32(b+24,0x2000u);u32(b+28,kind==1u?2040u:52u);u16(b+42,32);u16(b+44,1);
 u32(b+52,1u);u32(b+56,0x100u);u32(b+60,0x2000u);
 u32(b+68,kind==0u?3000u:4u);u32(b+72,kind==0u?3000u:16u);
 memcpy(b+0x100,"TEST",4);fwrite(b,1,2048,f);fclose(f);return 0;
}
int main(void){
 const char *path="/tmp/gekko2-test-disc-elf-guard-beta12.iso";
 bios_image_t bios={0};bios.data=calloc(1,BIOS_MAX_SIZE);bios.size=BIOS_MAX_SIZE;bios.loaded=1;
 if(!bios.data||system_init(&bios,&bios))return puts("FAIL system initialization"),1;
 ee_state_t *s=ee_core_get_state();uint32_t entry,gp;
 for(unsigned kind=0;kind<3;kind++){
  if(write_image(path,kind)||iop_cdvd_mount_iso(path))return puts("FAIL synthetic mount"),1;
  memset(s->ram+0x2000,0xa5,64);
  int ok=sif_loadfile_elf_load_disc(s,"TRUNC.ELF;1",&entry,&gp);
  if(kind<2){
   if(ok)return puts("FAIL malformed ELF accepted"),1;
   for(unsigned i=0;i<64;i++)if(s->ram[0x2000+i]!=0xa5)return puts("FAIL malformed ELF wrote RAM"),1;
  }else{
   if(!ok||entry!=0x2000u||memcmp(s->ram+0x2000,"TEST",4))return puts("FAIL valid ELF load"),1;
   for(unsigned i=4;i<16;i++)if(s->ram[0x2000+i])return puts("FAIL ELF BSS"),1;
  }
  iop_cdvd_unmount_iso();
 }
 remove(path);free(bios.data);
 puts("PASS synthetic LOADFILE: reject truncated program segment/header, accept exact valid copy and BSS");
 return 0;
}
