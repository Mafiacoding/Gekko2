#include "core/hw/gs_mem.h"
#include "core/hw/gs_gx.h"
#include <string.h>
#include <stdio.h>
static int calls,fail,reenter;
static uint8_t texture[128];
static int resolve(void *opaque,uint8_t *vram,uint32_t size)
{
    (void)opaque;calls++;
    if(fail)return 0;
    reenter=gs_mem_get()==NULL && gs_mem_sync()==0;
    return gs_gx_unpack_psmct32(vram,size,texture,sizeof(texture),64,128,63,31,8,4,0x80);
}
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    gs_mem_init();
    CHECK(!gs_mem_gpu_mark_pending());
    for(unsigned y=0;y<4;y++)for(unsigned x=0;x<8;x++) {
        unsigned t=(x/4)*64+y*8+(x%4)*2;
        texture[t]=255;texture[t+1]=(uint8_t)(x+1);
        texture[t+32]=(uint8_t)(y+11);texture[t+33]=23;
    }
    CHECK(gs_mem_gpu_bind(resolve,NULL));CHECK(gs_mem_gpu_mark_pending());
    CHECK(gs_mem_gpu_pending());CHECK(calls==0);
    CHECK(gs_mem_read_psmct32(64,128,63,31)==0x80170b01u);
    CHECK(calls==1&&reenter&&!gs_mem_gpu_pending());
    for(unsigned y=0;y<4;y++)for(unsigned x=0;x<8;x++)
        CHECK(gs_mem_read_psmct32(64,128,63+x,31+y)==(0x80170000u|((y+11)<<8)|(x+1)));
    CHECK(calls==1);
    CHECK(gs_mem_gpu_mark_pending());gs_mem_write_psmct32(64,128,63,31,0x12345678);
    CHECK(calls==2&&gs_mem_read_psmct32(64,128,63,31)==0x12345678);
    CHECK(gs_mem_gpu_mark_pending());uint32_t row[2];gs_mem_read_psmct32_span(row,64,128,63,31,2);
    CHECK(calls==3&&row[0]==0x80170b01u&&row[1]==0x80170b02u);
    fail=1;CHECK(gs_mem_gpu_mark_pending());
    CHECK(gs_mem_read_psmct32(64,128,63,31)==0);CHECK(gs_mem_get()==NULL);
    CHECK(!gs_mem_gpu_bind(NULL,NULL)&&gs_mem_gpu_pending());
    gs_mem_write_psmct32(64,128,63,31,0xdeadbeef);
    CHECK(gs_mem_gpu_pending()&&gs_mem_sync_failures()>=4);
    fail=0;CHECK(gs_mem_sync());CHECK(gs_mem_read_psmct32(64,128,63,31)==0x80170b01u);
    /* Every alias and write path must resolve before accessing raw bytes. */
#define BARRIER(expr) do {int n=calls;CHECK(gs_mem_gpu_mark_pending());(void)(expr);CHECK(calls==n+1&&!gs_mem_gpu_pending());} while(0)
    BARRIER(gs_mem_read_psmct16(64,128,0,0));
    BARRIER(gs_mem_read_psmct16s(64,128,0,0));
    BARRIER(gs_mem_read_psmct32_swizzled(0,128,0,0));
    BARRIER(gs_mem_read_z(64,128,0,0,0));
    BARRIER(gs_mem_read_index(64,128,0,0,0x13));
    BARRIER(gs_mem_read_index(64,128,0,0,0x14));
    BARRIER(gs_mem_read_index(64,128,0,0,0x1b));
    BARRIER(gs_mem_write_psmct16(64,128,0,0,1));
    BARRIER(gs_mem_write_psmct16s(64,128,0,0,2));
    BARRIER(gs_mem_write_psmct32_swizzled(0,128,0,0,3));
    BARRIER(gs_mem_write_z(64,128,0,0,1,4));
    BARRIER(gs_mem_write_index(64,128,0,0,0x13,5));
    BARRIER(gs_mem_write_index(64,128,0,0,0x14,6));
    BARRIER(gs_mem_write_index(64,128,0,0,0x2c,7));
    BARRIER(gs_mem_fill_psmct32_span(64,128,0,0,8,9));
    BARRIER(gs_mem_get());
#undef BARRIER
    uint8_t *vram=gs_mem_get();static uint8_t snapshot[GS_MEM_SIZE];memcpy(snapshot,vram,sizeof(snapshot));
    CHECK(!gs_gx_unpack_psmct32(vram,GS_MEM_SIZE,texture,127,64,128,63,31,8,4,128));
    CHECK(!gs_gx_unpack_psmct32(vram,GS_MEM_SIZE,texture,128,0xffffffffu,128,0,0,8,4,128));
    CHECK(!gs_gx_unpack_psmct32(vram,GS_MEM_SIZE,texture,128,64,128,0,0,7,4,128));
    CHECK(!gs_gx_unpack_psmct32(vram,GS_MEM_SIZE,texture,128,64,128,0,0,8,4,256));
    CHECK(memcmp(snapshot,vram,sizeof(snapshot))==0);
    CHECK(gs_mem_gpu_mark_pending());gs_mem_init();CHECK(!gs_mem_gpu_pending());
    CHECK(gs_mem_read_psmct32(64,128,63,31)==0);
    CHECK(gs_mem_gpu_bind(NULL,NULL));CHECK(!gs_mem_gpu_mark_pending());
    puts("PASS VRAM synchronization, deferred import, aliases/order, failure/retry, reentrancy, raw access and reset");return 0;
}
