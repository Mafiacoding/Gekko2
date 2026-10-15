/* BETA12.9: GS local-to-local move, overlapping direction and formats. */
#include <stdio.h>
#include "hw/gs_mem.c"
static int fails;
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
    printf("%s GS local-to-local copy, overlap, formats, bounds\n",fails?"FAIL":"PASS");
    return !!fails;
}
