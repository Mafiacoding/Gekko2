#include "core/ee/ee_core.c"
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){printf("FAIL line %u\n",__LINE__);return 1;}}while(0)
static void put(uint8_t*p,uint32_t v){for(unsigned n=0;n<4;n++)p[n]=(uint8_t)(v>>(8*n));}
int main(void){
 ee_state_t*s=calloc(1,sizeof(*s));CHECK(s);s->ram=calloc(1,65536);s->ram_size=65536;CHECK(s->ram);
 s->tlb[0].entry_hi=0x200000|7;s->tlb[0].entry_lo0=(2<<6)|2;s->tlb[0].entry_lo1=(3<<6)|2;s->cop0[10]=7;
 put(s->ram+0x2000,0x24020007);put(s->ram+0x3000,0x24020009);
 CHECK(ee_fetch32(s,0x200000)==0x24020007&&!s->mem_tlb_miss);
 CHECK(ee_fetch32(s,0x201000)==0x24020009&&!s->mem_tlb_miss);
 s->tlb[0].entry_lo0=(3<<6)|2;CHECK(ee_fetch32(s,0x200000)==0x24020009);
 put(s->ram+0x3000,0x24020011);CHECK(ee_fetch32(s,0x200000)==0x24020011); /* self-modifying code */
 s->cop0[10]=8;CHECK(ee_fetch32(s,0x200000)==0&&s->mem_tlb_miss);
 s->cop0[10]=7;s->tlb[0].entry_hi=0x400007;CHECK(ee_fetch32(s,0x200000)==0&&s->mem_tlb_miss);s->tlb[0].entry_hi=0x200007;
 s->tlb[0].entry_lo0=(16<<6)|2;CHECK(ee_fetch32(s,0x200000)==0&&!s->mem_tlb_miss); /* backed boundary */
 s->tlb[0].entry_lo0=(3<<6)|3;s->tlb[0].entry_lo1|=1;s->cop0[10]=8;
 CHECK(ee_fetch32(s,0x200000)==0x24020011); /* both G bits bypass ASID */
 for(unsigned n=0;n<4;n++){put(s->ram+0x3000+n,0x91827364+n);CHECK(ee_fetch32(s,0x200000+n)==0x91827364+n);}
 free(s->ram);free(s);puts("PASS mapped fetch: even/odd, TLB replacement, SMC, ASID, VPN/global flags, bounds and unaligned byte order");return 0;
}
