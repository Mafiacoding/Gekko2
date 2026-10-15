/* BETA12.9: GS local-to-local move, overlapping direction and formats. */
#include <stdio.h>
#include "hw/gs_mem.c"
static int fails;
static int deny_gpu_readback(void *opaque, uint8_t *vram, uint32_t bytes)
{ (void)opaque; (void)vram; (void)bytes; return 0; }
#define EXPECT(e,msg) do {if(!(e)){fprintf(stderr,"FAIL: %s\n",msg);fails++;}} while(0)
static gs_local_copy_desc_t mk(unsigned sp,unsigned dp,unsigned psm)
{
    gs_local_copy_desc_t d={0};
    d.sbp=sp;d.dbp=dp;d.sbw=d.dbw=64;
    d.spsm=d.dpsm=psm;d.rrw=4;d.rrh=2;
    return d;
}
int main(void)
{
    gs_mem_init();
    gs_local_copy_desc_t d=mk(0,128,0);
    for(unsigned y=0;y<2;y++)for(unsigned x=0;x<4;x++)
        gs_mem_write_psmct32_blk(0,64,x,y,0xff220000u+y*16+x);
    EXPECT(gs_mem_copy_local(&d),"cross-buffer 32-bit supported");
    for(unsigned y=0;y<2;y++)for(unsigned x=0;x<4;x++)
        EXPECT(gs_mem_read_psmct32_blk(128,64,x,y)==0xff220000u+y*16+x,"source color preserved");
    /* The same surface, shifted right by one pixel: right-to-left
     * direction must preserve the entire row rather than repeat pixel 0. */
    d=mk(0,0,0);d.rrh=1;d.dsax=1;d.dirx=1;
    for(unsigned x=0;x<5;x++)gs_mem_write_psmct32_blk(0,64,x,5,100+x);
    d.ssay=5;d.dsay=5;
    EXPECT(gs_mem_copy_local(&d),"overlap right-to-left supported");
    for(unsigned x=0;x<4;x++)EXPECT(gs_mem_read_psmct32_blk(0,64,x+1,5)==100+x,"right-to-left overlap correct");
    /* Moving down one line within same framebuffer copies bottom-up. */
    d=mk(0,0,0);d.rrw=1;d.rrh=3;d.dsay=1;d.diry=1;
    for(unsigned y=0;y<4;y++)gs_mem_write_psmct32_blk(0,64,10,y,200+y);
    d.ssax=d.dsax=10;
    EXPECT(gs_mem_copy_local(&d),"overlap bottom-up supported");
    for(unsigned y=0;y<3;y++)EXPECT(gs_mem_read_psmct32_blk(0,64,10,y+1)==200+y,"bottom-up overlap correct");
    /* Some titles perform an overlapping local copy without DIRX/DIRY;
     * mirroring pixel 0 across the row is not allowed. */
    d=mk(0,0,0);d.rrh=1;d.ssax=0;d.dsax=1;d.ssay=d.dsay=9;
    d.dirx=d.diry=0;
    for(unsigned x=0;x<5;x++)gs_mem_write_psmct32_blk(0,64,x,9,300+x);
    EXPECT(gs_mem_copy_local(&d),"overlap without DIRX supported");
    for(unsigned x=0;x<4;x++)EXPECT(gs_mem_read_psmct32_blk(0,64,x+1,9)==300+x,"overlap auto right-to-left");
    /* Source and destination overlap vertically, despite DIRY=0. */
    d=mk(0,0,0);d.rrw=1;d.rrh=3;d.dsax=d.ssax=15;d.dsay=1;
    d.dirx=d.diry=0;
    for(unsigned y=0;y<4;y++)gs_mem_write_psmct32_blk(0,64,15,y,400+y);
    EXPECT(gs_mem_copy_local(&d),"vertical overlap without DIRY supported");
    for(unsigned y=0;y<3;y++)EXPECT(gs_mem_read_psmct32_blk(0,64,15,y+1)==400+y,"overlap auto bottom-up");
    /* Reversing a disjoint copy is supported without changing semantics. */
    d=mk(0,128,0); d.rrh=1;d.dirx=1;d.diry=1;
    for(unsigned x=0;x<4;x++)gs_mem_write_psmct32_blk(0,64,x,0,500+x);
    EXPECT(gs_mem_copy_local(&d),"direction with distinct surfaces supported");
    for(unsigned x=0;x<4;x++)EXPECT(gs_mem_read_psmct32_blk(128,64,x,0)==500+x,"non-overlap directional semantics");
    /* PSMCT24 must not overwrite alpha byte in the destination. */
    d=mk(0,128,1);d.rrw=d.rrh=1;d.ssax=30;d.dsax=31;
    gs_mem_write_psmct32_blk(0,64,30,0,0x00112233);
    gs_mem_write_psmct32_blk(128,64,31,0,0xab998877);
    EXPECT(gs_mem_copy_local(&d),"PSMCT24 move supported");
    EXPECT(gs_mem_read_psmct32_blk(128,64,31,0)==0xab112233,"24-bit alpha kept");
    /* Do not silently reinterpret incompatible PSMs or unsafe coords. */
    d.dpsm=2;
    EXPECT(!gs_mem_copy_local(&d),"format conversion safely declined");
    d=mk(0,128,0);d.ssax=2047;d.rrw=4;
    EXPECT(!gs_mem_copy_local(&d),"unsafe rectangle rejected");
    /* 16-bit shifted swizzle is supported as its own format. */
    d=mk(0,128,10);d.rrw=d.rrh=1;
    gs_mem_write_psmct16s(0,64,3,4,0x6a35u);
    d.ssax=3;d.ssay=4;d.dsax=5;d.dsay=6;
    EXPECT(gs_mem_copy_local(&d),"PSMCT16S same-layout move supported");
    EXPECT(gs_mem_read_psmct16s(128*64,64,5,6)==0x6a35u,"16S correct swizzle copied");
    /* Indexed PSMT8 copies an index, not a palette-expanded RGB value. */
    d=mk(0,128,0x13);d.rrw=d.rrh=1;
    d.ssax=7;d.ssay=5;d.dsax=8;d.dsay=6;
    gs_mem_write_index(0,64,7,5,0x13,0xa5u);
    EXPECT(gs_mem_copy_local(&d),"PSMT8 local copy supported");
    EXPECT(gs_mem_read_index(128*64,64,8,6,0x13)==0xa5u,"8-bit index unchanged");
    /* All-or-nothing GPU ownership: never succeed with failed readback. */
    gs_mem_gpu_bind(deny_gpu_readback,NULL);
    EXPECT(gs_mem_gpu_mark_pending(),"test deferred GPU ownership");
    d=mk(0,128,0);d.rrw=d.rrh=1;
    EXPECT(!gs_mem_copy_local(&d),"readback failure declines local copy");
    EXPECT(gs_mem_gpu_pending(),"failed transfer keeps GPU pending");
    printf("%s GS local-to-local copy, overlap, formats, bounds\n",fails?"FAIL":"PASS");
    return !!fails;
}
