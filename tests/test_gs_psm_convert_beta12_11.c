/* Host-only regression: checks native swizzle-compatible moves and the
 * explicitly opt-in non-hardware cross-bitdepth path. */
#include <stdio.h>
#include "hw/gs_mem.c"
static int failed;
#define T(x) do{if(!(x)){fprintf(stderr,"FAIL L%d: %s\n",__LINE__,#x);failed++;}}while(0)
static gs_local_copy_desc_t dsc(unsigned sp,unsigned dp,unsigned s,unsigned d) {
    gs_local_copy_desc_t o={0};o.sbp=sp;o.dbp=dp;o.sbw=o.dbw=128;
    o.spsm=s;o.dpsm=d;o.rrw=o.rrh=1;return o;
}
int main(void) {
    gs_mem_init();
    gs_local_copy_desc_t d=dsc(0,128,2,10);
    gs_mem_write_psmct16(0,128,0,0,0x932du);
    T(gs_mem_copy_local(&d));
    T(gs_mem_read_psmct16s(128*64,128,0,0)==0x932du);
    d=dsc(0,128,0,0x30);
    gs_mem_write_psmct32(0,128,2,3,0xdeadbeefu);
    d.ssax=d.dsax=2;d.ssay=d.dsay=3;
    T(gs_mem_copy_local(&d));
    T(gs_mem_read_z(128*64,128,2,3,0)==0xdeadbeefu);
    /* Read-only alpha in CT24 should remain unchanged. */
    d=dsc(0,128,1,1);d.ssax=d.dsax=10;
    gs_mem_write_psmct32(0,128,10,0,0xff112233u);
    gs_mem_write_psmct32(128*64,128,10,0,0xac445566u);
    T(gs_mem_copy_local(&d));
    T(gs_mem_read_psmct32(128*64,128,10,0)==0xac112233u);
    /* Same-bpp indexed formats: index not palette-expansion. */
    d=dsc(0,128,0x13,0x1b);d.ssax=d.dsax=1;
    gs_mem_write_index(0,128,1,0,0x13,0xa5u);
    T(gs_mem_copy_local(&d));
    T(gs_mem_read_index(128*64,128,1,0,0x1b)==0xa5u);
    /* The real GS-facing path does not invent lossy conversions. */
    d=dsc(0,128,0,2);d.ssax=d.dsax=8;
    gs_mem_write_psmct32(0,128,8,0,0x8000ff00u);
    T(!gs_mem_copy_local(&d));
    T(gs_mem_local_rejected_formats()>0);
    /* A separate experimental API switch enables 32->16, 16->32. */
    gs_mem_set_local_extended_conversion(1);
    T(gs_mem_copy_local(&d));
    T(gs_mem_read_psmct16(128*64,128,8,0)==0x83e0u);
    d=dsc(128,256,2,0);d.ssax=d.dsax=8;
    T(gs_mem_copy_local(&d));
    T(gs_mem_read_psmct32(256*64,128,8,0)==0x8000ff00u);
    /* 32->24 alpha untouched; optional conversion must set 24->32 alpha. */
    d=dsc(0,256,0,1);d.ssax=d.dsax=8;
    gs_mem_write_psmct32(256*64,128,8,0,0xee112233u);
    T(gs_mem_copy_local(&d));
    T(gs_mem_read_psmct32(256*64,128,8,0)==0xee00ff00u);
    d=dsc(256,384,1,0);d.ssax=d.dsax=8;
    T(gs_mem_copy_local(&d));
    T(gs_mem_read_psmct32(384*64,128,8,0)==0x8000ff00u);
    /* Cross-format aliasing cannot be silently corrupted. */
    d=dsc(0,0,0,2);d.ssax=d.dsax=8;
    T(!gs_mem_copy_local(&d));
    /* Index 8->4 truncates only in explicitly enabled experimental mode. */
    d=dsc(0,128,0x13,0x14);d.ssax=d.dsax=3;
    gs_mem_write_index(0,128,3,0,0x13,0xbau);
    T(gs_mem_copy_local(&d));
    T(gs_mem_read_index(128*64,128,3,0,0x14)==0xau);
    gs_mem_set_local_extended_conversion(0);
    T(!gs_mem_copy_local(&d));
    printf("%s BETA12.11 PSM move / strict and experimental conversions\n",failed?"FAIL":"PASS");
    return !!failed;
}
