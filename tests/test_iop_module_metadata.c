#include <stdio.h>
#include <string.h>
#include "hw/iop_module_metadata.c"
static unsigned fail;
#define CHECK(c,m) do{if(!(c)){printf("FAIL: %s\n",m);fail++;}}while(0)
static void put32(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=v>>(i*8);}
int main(void){
 uint8_t image[160]={0};char name[64];uint16_t version=0;
 memcpy(image,"\177ELF",4);image[4]=image[5]=1;put32(image+32,52);image[46]=40;image[48]=1;
 put32(image+56,0x70000080);put32(image+68,100);put32(image+72,40);
 image[124]=0x0b;image[125]=2;memcpy(image+126,"mcman_tool",11);
 CHECK(iop_module_metadata(image,sizeof image,name,&version)&&version==0x20b&&!strcmp(name,"mcman_tool"),"reads version and name from IOPMOD section");
 CHECK(!iop_module_metadata(image,120,name,&version),"rejects truncated IOPMOD extent");
 put32(image+68,0xfffffff0);CHECK(!iop_module_metadata(image,sizeof image,name,&version),"rejects overflowing section extent");put32(image+68,100);
 image[5]=2;CHECK(!iop_module_metadata(image,sizeof image,name,&version),"rejects big-endian ELF");image[5]=1;
 put32(image+32,0xfffffff0);CHECK(!iop_module_metadata(image,sizeof image,name,&version),"rejects overflowing section table");
 printf("IOP metadata %s\n",fail?"FAIL":"PASS");return fail?1:0;
}
