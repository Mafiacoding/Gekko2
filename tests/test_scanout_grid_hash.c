#include <stdint.h>
#include <stdio.h>
#include "core/hw/gs_mem.h"
#include "core/hw/gs_wii_output.h"
static unsigned checks;
#define CHECK(e) do { checks++; if(!(e)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#e);return 1;} } while(0)
static int fail_readback(void *p,uint8_t *mem,uint32_t n)
{(void)p;(void)mem;(void)n;return 0;}
int main(void)
{
    uint32_t a,b,c;
    gs_mem_init();
    CHECK(gs_scanout_grid_hash(0,64,0,0,8,8,0,&a));
    gs_mem_write_psmct32(0,64,3,4,0x80000000u);
    CHECK(gs_scanout_grid_hash(0,64,0,0,8,8,0,&b)&&a==b);
    gs_mem_write_psmct32(0,64,3,4,0x800000ffu);
    CHECK(gs_scanout_grid_hash(0,64,0,0,8,8,0,&b)&&a!=b);
    CHECK(gs_scanout_grid_hash(0,64,0,0,8,8,1,&c)&&b==c);
    CHECK(gs_scanout_grid_hash(2048,64,12,39,8,8,2,&a));
    gs_mem_write_psmct16(2048,64,15,43,0x801fu);
    CHECK(gs_scanout_grid_hash(2048,64,12,39,8,8,2,&b)&&a!=b);
    CHECK(gs_scanout_grid_hash(4096,64,12,39,8,8,10,&a));
    gs_mem_write_psmct16s(4096,64,15,43,0x03e0u);
    CHECK(gs_scanout_grid_hash(4096,64,12,39,8,8,10,&b)&&a!=b);
    CHECK(!gs_scanout_grid_hash(0,64,0,0,8,8,3,&c)&&c==0);
    CHECK(!gs_scanout_grid_hash(0,0,0,0,8,8,0,&c)&&c==0);
    CHECK(!gs_scanout_grid_hash(0,64,UINT32_MAX,0,8,8,0,&c)&&c==0);
    CHECK(!gs_scanout_grid_hash(0,64,0,0,0,8,0,&c)&&c==0);
    CHECK(gs_mem_gpu_bind(fail_readback,NULL));
    CHECK(gs_mem_gpu_protect_range(0,8192));
    CHECK(gs_mem_gpu_mark_pending());
    CHECK(!gs_scanout_grid_hash(0,64,0,0,8,8,0,&c)&&c==0);
    CHECK(gs_mem_gpu_pending());
    CHECK(gs_mem_sync_failures()==1); /* Stop at the first failed sample. */
    printf("PASS scanout grid hash: %u checks (RGB/PSM/offset/bounds/readback)\n",checks);
    return 0;
}
