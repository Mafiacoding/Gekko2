#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "core/recompiler/ee_jit.c"

static int fail(const char *m){fprintf(stderr,"R1318 cache eviction test: %s\n",m);return 1;}
static ee_precise_fn alloc_fn(void){return (ee_precise_fn)malloc(32u);}
int main(void)
{
    uint32_t a[2]={0x24020001u,0x24420001u};
    uint32_t b[2]={0x24030002u,0x24630001u};
    ee_precise_reset_cache();
    uint32_t pc1=0x00200000u,pc2=pc1+4u;
    while(ee_precise_cache_index(pc2)!=ee_precise_cache_index(pc1)) pc2+=4u;
    if(pc1==pc2) return fail("collision search");
    ee_precise_slot *slot=&precise_cache[ee_precise_cache_index(pc1)];
    ee_precise_fn f1=alloc_fn(),f2=alloc_fn();
    if(!f1||!f2)return fail("allocation");
    if(!ee_precise_install_slot(slot,pc1,a,2u,3u,7u,11u,f1))return fail("first install");
    uint32_t serial1=slot->serial;
    if(!serial1||precise_evictions) return fail("initial identity/counter");
    precise_active=1;
    if(ee_precise_install_slot(slot,pc2,b,2u,4u,8u,12u,f2))return fail("live replacement accepted");
    if(slot->fn!=f1||slot->pc!=pc1||slot->serial!=serial1)return fail("live slot mutated");
    precise_active=0;
    if(!ee_precise_install_slot(slot,pc2,b,2u,4u,8u,12u,f2))return fail("collision replacement");
    if(slot->fn!=f2||slot->pc!=pc2||!slot->serial||slot->serial==serial1)return fail("replacement identity");
    if(precise_evictions!=1u)return fail("eviction count");
    precise_serial_source=UINT32_MAX;
    ee_precise_fn f3=alloc_fn();if(!f3)return fail("wrap allocation");
    ee_precise_slot *other=&precise_cache[(ee_precise_cache_index(pc1)+1u)&255u];
    if(!ee_precise_install_slot(other,pc1+0x1000u,a,2u,5u,9u,13u,f3))return fail("wrap install");
    if(other->serial!=1u)return fail("serial wrap did not skip zero");
    precise_active=1;ee_precise_reset_cache();
    if(!slot->fn||!other->fn)return fail("active reset released live code");
    precise_active=0;ee_precise_reset_cache();
    if(slot->fn||other->fn||precise_serial_source||precise_evictions)return fail("inactive reset");
    puts("R1318 cache eviction test: PASS");
    return 0;
}
