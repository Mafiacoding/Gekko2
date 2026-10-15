#include <stdio.h>
#include <stdint.h>
#include "core/recompiler/ee_jit.c"

static int fail(const char *m){fprintf(stderr,"R1316 generation test: %s\n",m);return 1;}
int main(void)
{
    ee_precise_reset_cache();
    if(precise_page_generation[0]||precise_page_generation[1]||precise_mapping_generation) return fail("reset");
    ee_jit_notify_physical_write(0x00000fffu,2u);
    if(precise_page_generation[0]!=1u||precise_page_generation[1]!=1u) return fail("cross-page write");
    ee_jit_notify_physical_write(0x00002000u,0u);
    if(precise_page_generation[2]!=0u) return fail("zero-length write");
    ee_jit_notify_physical_write(32u*1024u*1024u-1u,8u);
    if(precise_page_generation[EE_PRECISE_RAM_PAGES-1u]!=1u) return fail("RAM-end clipping");
    ee_jit_notify_physical_write(32u*1024u*1024u,4u);
    if(precise_page_generation[EE_PRECISE_RAM_PAGES-1u]!=1u) return fail("out-of-RAM write");
    ee_jit_notify_mapping_change();
    ee_jit_notify_mapping_change();
    if(precise_mapping_generation!=2u) return fail("mapping epoch");
    puts("R1316 generation test: PASS");
    return 0;
}
