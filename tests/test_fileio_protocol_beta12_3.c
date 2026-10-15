/* No copyrighted module is bundled: synthetic ELF metadata + real ABI assertions. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "core/hw/iop_module_metadata.h"
static void u32(uint8_t*p,uint32_t x){for(unsigned i=0;i<4;i++)p[i]=x>>(8*i);}
int main(int argc,char **argv) {
 uint8_t mod[256]={0};uint32_t version=0;
 memcpy(mod,"\177ELF\1\1",6);mod[18]=8;u32(mod+32,52);mod[46]=40;mod[48]=1;
 u32(mod+56,0x70000080);u32(mod+68,112);u32(mod+72,64);
 memcpy(mod+112+26,"FILEIO_service",15);mod[112+24]=7;mod[112+25]=2;
 memcpy(mod+190,"PsIIfileio  2110",16);
 if(!iop_fileio_protocol(mod,sizeof mod,&version)||version!=0x30313132u)return puts("FAIL: 2110 synthetic"),1;
 memcpy(mod+190,"PsIIfileio  2050",16);
 if(!iop_fileio_protocol(mod,sizeof mod,&version)||version!=0x30353032u)return puts("FAIL: 2050 synthetic"),1;
 memcpy(mod+190,"PsIIfileio  abcd",16);
 if(iop_fileio_protocol(mod,sizeof mod,&version))return puts("FAIL: accepted non-decimal"),1;
 memcpy(mod+190,"PsIIfileio  2110",16);
 if(iop_fileio_protocol(mod,200,&version))return puts("FAIL: accepted truncated payload"),1;
 memcpy(mod+112+26,"BAD___service",13);
 if(iop_fileio_protocol(mod,sizeof mod,&version))return puts("FAIL: accepted wrong module name"),1;
 memcpy(mod+112+26,"FILEIO_service",15);
 if(iop_fileio_protocol(mod,60,&version))return puts("FAIL: accepted truncated ELF"),1;
 puts("PASS: version literals, bounds, module identity, invalid data");
 if(argc>1) {
   for(int i=1;i<argc;i++){
     FILE*f=fopen(argv[i],"rb");if(!f)return perror(argv[i]),1;
     fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
     if(n<52||n>1000000)return puts("FAIL: input size"),1;
     uint8_t *buf=__builtin_malloc((size_t)n);
     if(!buf||fread(buf,1,(size_t)n,f)!=(size_t)n)return puts("FAIL: read"),1;
     fclose(f);
     if(!iop_fileio_protocol(buf,(uint32_t)n,&version))return printf("FAIL: real module %s\n",argv[i]),1;
     printf("PASS private module %s: version %c%c%c%c\n",argv[i],(int)(version&255),(int)((version>>8)&255),(int)((version>>16)&255),(int)((version>>24)&255));
     __builtin_free(buf);
   }
 }
 return 0;
}
