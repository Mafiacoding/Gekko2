#include <assert.h>
#include <stdio.h>
#include "hw/gs_mem.c"
int main(void)
{
 gs_mem_init();
 uint32_t bp=(GS_MEM_SIZE/256u)-32u; /* final GS 8-KiB page */
 gs_mem_write_psmct32_blk(bp,64,3,0,0x11223344);
 gs_mem_write_psmct32_blk(bp,64,3,32,0xaabbccdd); /* next page wraps to offset zero */
 assert(gs_mem_read_psmct32_blk(bp,64,3,0)==0x11223344);
 assert(gs_mem_read_psmct32_blk(bp,64,3,32)==0xaabbccdd);
 assert(gs_mem_read_psmct32_blk(0,64,3,0)==0xaabbccdd);
 uint32_t span[5]={0};gs_mem_read_psmct32_span(span,bp*64u,64,1,32,5);
 assert(span[2]==0xaabbccdd);
 /* PSMCT16 / indexed views also map to the circular physical eDRAM. */
 gs_mem_write_psmct16(bp*64u,64,2,64,0x9bc7);
 assert(gs_mem_read_psmct16(bp*64u,64,2,64)==0x9bc7);
 gs_mem_write_index(bp*64u,128,4,64,0x13,0xe9);
 assert(gs_mem_read_index(bp*64u,128,4,64,0x13)==0xe9);
 uint64_t lo=9,hi=9;
 assert(local_page_range(bp,64,0,32,32,32,0,&lo,&hi));
 assert(lo==0 && hi==GS_MEM_SIZE); /* conservative overlap envelope */
 gs_local_copy_desc_t d={0};
 d.sbp=bp;d.dbp=32;d.sbw=d.dbw=64;d.spsm=d.dpsm=0;
 d.ssay=32;d.rrw=8;d.rrh=1;
 assert(gs_mem_copy_local(&d));
 assert(gs_mem_read_psmct32_blk(32,64,3,0)==0xaabbccdd);
 puts("PASS GS 4-MiB ring wrap, scalar/span/index and local-copy alias");
}
