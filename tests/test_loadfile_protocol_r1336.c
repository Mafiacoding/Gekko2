/* Synthetic ELF with a literal legacy RPC getter; no module/BIOS bytes. */
#include "core/hw/iop_module_metadata.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static void put(uint8_t*p,uint32_t v){for(unsigned n=0;n<4;n++)p[n]=v>>(8*n);}
int main(void){
 uint8_t b[512]={0};uint32_t version;
 memcpy(b,"\177ELF\1\1",6);b[18]=8;put(b+28,52);b[42]=32;b[44]=1;
 put(b+32,84);b[46]=40;b[48]=1;
 put(b+52,1);put(b+56,128);put(b+60,0x1000);put(b+68,384);
 put(b+88,1);put(b+92,6);put(b+100,128);put(b+104,64);
 uint32_t code[]={0x240200ff,0x14820006,0x00001021,0x3c030000,0x8c631100,0x3c020000,0x24421f80,0xac430000};
 for(unsigned n=0;n<8;n++)put(b+128+4*n,code[n]);memcpy(b+384,"1234",4);
 assert(iop_loadfile_protocol(b,sizeof b,&version)&&version==0x34333231);
 assert(!iop_loadfile_protocol(b,160,&version));
 b[387]='x';assert(!iop_loadfile_protocol(b,sizeof b,&version));b[387]='4';
 put(b+144,0x8c631ff0);assert(!iop_loadfile_protocol(b,sizeof b,&version));put(b+144,code[4]);
 put(b+132,0x14820005);assert(!iop_loadfile_protocol(b,sizeof b,&version));put(b+132,code[1]);
 put(b+56,0xfffffff0);assert(!iop_loadfile_protocol(b,sizeof b,&version));put(b+56,128);
 b[18]=20;assert(!iop_loadfile_protocol(b,sizeof b,&version));
 puts("PASS literal getter, unknown protocol, instruction mismatch, ELF extents and truncated/overflowing input");
}
