#include "core/hw/gs_mem.h"
#include "core/recompiler/optimization.h"
#include <assert.h>
#include <stdio.h>
uint32_t gekko2_optimization_mask=GEKKO2_OPT_DEFAULT;
static unsigned calls;static int fail;
static int resolve(void *opaque,uint8_t *vram,uint32_t size)
{
 (void)opaque;assert(size==GS_MEM_SIZE);calls++;
 if(fail)return 0;
 vram[0]=0x77;vram[1]=0x66;vram[2]=0x55;vram[3]=0x44;return 1;
}
static void pending(void)
{
 assert(gs_mem_gpu_bind(resolve,NULL));assert(gs_mem_gpu_protect_range(0,8192));
 assert(gs_mem_gpu_mark_pending());
}
int main(void)
{
 gs_mem_init();pending();unsigned before=calls;
 gs_mem_write_psmct32(8192,64,0,0,0x12345678);
 gs_mem_write_psmct32_swizzled(8,64,1,0,0xaabbccdd);
 gs_mem_write_psmct16(8192,64,2,0,0x789a);
 gs_mem_write_psmct16s(8192,64,3,0,0xbcde);
 gs_mem_write_index(8192,64,4,0,0x13,0x5a);
 gs_mem_write_index(8192,64,5,0,0x14,0xb);
 gs_mem_write_index(8192,64,6,0,0x1b,0xef);
 gs_mem_fill_psmct32_span(8192,64,0,32,130,0x88776655);
 assert(calls==before&&gs_mem_gpu_pending());
 assert(gs_mem_read_psmct32(8192,64,0,0)==0x12345678);
 assert(gs_mem_read_psmct32_swizzled(8,64,1,0)==0xaabbccdd);
 assert(gs_mem_read_psmct16(8192,64,2,0)==0x789a);
 assert(gs_mem_read_psmct16s(8192,64,3,0)==0xbcde);
 assert(gs_mem_read_index(8192,64,4,0,0x13)==0x5a);
 assert(gs_mem_read_index(8192,64,5,0,0x14)==0xb);
 assert(gs_mem_read_index(8192,64,6,0,0x1b)==0xef);
 assert(gs_mem_read_psmct32(8192,64,129,32)==0x88776655);
 gs_mem_write_psmct32(0,64,0,0,0x10203040);
 assert(calls==before+1&&!gs_mem_gpu_pending()&&gs_mem_read_psmct32(0,64,0,0)==0x10203040);
 assert(gs_mem_read_psmct32(8192,64,0,0)==0x12345678);
 pending();before=calls;assert(gs_mem_get());assert(calls==before+1);
 pending();fail=1;before=calls;
 gs_mem_write_psmct32(8192,64,0,0,0x87654321);
 assert(calls==before&&gs_mem_read_psmct32(8192,64,0,0)==0x87654321);
 gs_mem_write_psmct16(0,64,0,0,0xdead);assert(calls==before+1&&gs_mem_gpu_pending());
 fail=0;assert(gs_mem_sync());assert(gs_mem_read_psmct32(0,64,0,0)==0x44556677);
 pending();before=calls;gekko2_optimization_mask&=~GEKKO2_OPT_BIT(GEKKO2_OPT_GX_DISJOINT_WRITES);
 gs_mem_write_psmct32(8192,64,0,0,0x55);assert(calls==before+1&&!gs_mem_gpu_pending());
 assert(gs_mem_gpu_bind(NULL,NULL));puts("PASS disjoint color/16-bit/index/span writes, alias/raw fences, failure and control mode");
}
