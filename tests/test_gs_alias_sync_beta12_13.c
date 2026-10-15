/* BETA12.13: shared eDRAM PSM-alias and deferred GX coherence regression. */
#include <stdio.h>
#include <stdint.h>
#include "hw/gs_mem.c"
static int bad;
#define CHECK(e) do { if(!(e)){ fprintf(stderr,"FAILED line %d: %s\n",__LINE__,#e);bad++; }}while(0)
static int resolve_ok(void *opaque,uint8_t *mem,uint32_t bytes)
{ unsigned *calls=(unsigned *)opaque;(*calls)++;if(bytes)mem[0]=0x5a;return 1; }
static int resolve_fail(void *opaque,uint8_t *mem,uint32_t bytes)
{ (void)opaque;(void)mem;(void)bytes;return 0; }
int main(void)
{
    gs_mem_init();
    /* Real CT16 pages span 64 rows, not 128. */
    uint64_t lo=0,hi=0;
    CHECK(local_page_range(0,64,0,0,1,64,2,&lo,&hi));
    CHECK(lo==0 && hi==8192u);
    CHECK(local_page_range(0,64,0,64,1,1,10,&lo,&hi));
    CHECK(lo==8192u && hi==16384u);
    CHECK(local_page_range(0,128,0,64,1,1,0x13u,&lo,&hi));
    CHECK(lo==8192u && hi==16384u); /* index8: 128-wide pages */
    /* The Z32 and CT32 views physically share their 8-KiB page.
       A CT32->Z32 move must preserve *all* source bytes before its first
       destination store, rather than overwrite later unread source texels. */
    gs_local_copy_desc_t d={0};
    d.sbp=d.dbp=0;d.sbw=d.dbw=64;d.spsm=0;d.dpsm=0x30;
    d.rrw=64;d.rrh=32;
    for(uint32_t y=0;y<32;y++)for(uint32_t x=0;x<64;x++)
        gs_mem_write_psmct32(0,64,x,y,0x12000000u+y*64u+x);
    CHECK(gs_mem_copy_local(&d));
    for(uint32_t y=0;y<32;y++)for(uint32_t x=0;x<64;x++)
        CHECK(gs_mem_read_z(0,64,x,y,0u)==0x12000000u+y*64u+x);
    CHECK(gs_mem_local_alias_stat(0)==1);
    CHECK(gs_mem_local_alias_stat(1)==2048);
    d.rrh=257;CHECK(!gs_mem_copy_local(&d));
    CHECK(gs_mem_local_alias_stat(2)==1);
    /* Deferred GPU ownership is transferred only if source shares the
       physically protected framebuffer envelope. */
    unsigned calls=0;
    CHECK(gs_mem_gpu_bind(resolve_ok,&calls));
    CHECK(gs_mem_gpu_protect_range(0,8192));
    CHECK(gs_mem_gpu_mark_pending());
    CHECK(gs_mem_gpu_sync_alias_range(8192,16384));
    CHECK(gs_mem_gpu_pending()==1 && calls==0);
    CHECK(gs_mem_gpu_sync_alias_range(0,32));
    CHECK(!gs_mem_gpu_pending()&&calls==1);
    CHECK(gs_mem_alias_stat(0)==2&&gs_mem_alias_stat(1)==1);
    CHECK(gs_mem_alias_stat(2)==1);
    CHECK(gs_mem_gpu_bind(resolve_fail,NULL));
    CHECK(gs_mem_gpu_protect_range(0,8192));
    CHECK(gs_mem_gpu_mark_pending());
    CHECK(!gs_mem_gpu_sync_alias_range(0,16));
    CHECK(gs_mem_gpu_pending());
    CHECK(gs_mem_alias_stat(3)==1);
    puts(bad?"FAIL GS alias and framebuffer coherence":"PASS GS alias and framebuffer coherence");
    return !!bad;
}
