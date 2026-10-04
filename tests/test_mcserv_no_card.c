/* Cardless GETINFO service result and extra DMA before the guest callback. */
#include <stdio.h>
#include "core/ee/ee_core.c"
static unsigned fails;
#define CHECK(x,msg) do{if(!(x)){printf("FAIL: %s\n",msg);fails++;}}while(0)
int main(void){bios_image_t b={0};if(system_init(&b,&b))return 2;ee_state_t*s=ee_core_get_state();uint32_t req=0xa0110000,param=0xa0120000;
for(unsigned n=0;n<48;n+=4)ee_mem_write32(s,req+n,0);
ee_mem_write32(s,req+12,1);ee_mem_write32(s,req+16,1);ee_mem_write32(s,req+20,1);ee_mem_write32(s,req+28,0x20120000);
for(unsigned n=0;n<196;n+=4)ee_mem_write32(s,param+n,0xaabbccdd);
CHECK(ee_mcserv_getinfo_no_card(s,req,48,1)==-11,"no card returns failed probe rather than successful unformatted card");
CHECK(ee_mem_read32(s,param)==0&&ee_mem_read32(s,param+4)==0&&ee_mem_read32(s,param+144)==0,"modern callback receives no type, free clusters, format");
CHECK(ee_mem_read32(s,param+8)==0xaabbccdd&&ee_mem_read32(s,param+192)==0xaabbccdd,"does not overwrite unrelated callback memory");
ee_mem_write32(s,param,0xabc);ee_mem_write32(s,req+20,0);ee_mcserv_getinfo_no_card(s,req,48,1);CHECK(ee_mem_read32(s,param)==0xabc,"unrequested type output is untouched");
ee_mem_write32(s,param+144,0xdef);ee_mcserv_getinfo_no_card(s,req,48,0);CHECK(ee_mem_read32(s,param)==0&&ee_mem_read32(s,param+144)==0xdef,"legacy layout uses size for type and does not write modern format");
ee_mem_write32(s,param,0x123);CHECK(ee_mcserv_getinfo_no_card(s,req,12,1)==-11&&ee_mem_read32(s,param)==0x123,"truncated descriptor cannot trigger DMA");
ee_mem_write32(s,req+28,0x1ffffff0);CHECK(ee_mcserv_getinfo_no_card(s,req,48,1)==-11,"rejects out of RAM DMA destination");printf("MCSERV no-card: %u failures\n",fails);return fails?1:0;}
