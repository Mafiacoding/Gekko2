#define GEKKO2_EE_CHAIN_RESIDENCY
#include "core/recompiler/ee_jit.c"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    const uint32_t words[]={0x24840001u,0x24840001u};
    ee_precise_reset_cache();
    ee_precise_slot *slot=&precise_cache[0];
    ee_precise_fn first=(ee_precise_fn)malloc(32),second=(ee_precise_fn)malloc(32);
    assert(first&&second);
    assert(ee_precise_install_slot(slot,0x200000,words,2,0,1,2,first));
    slot->resident_fn=(ee_precise_fn)malloc(32);assert(slot->resident_fn);
    slot->resident_tested=1;
    precise_active=1;
    assert(!ee_precise_install_slot(slot,0x200100,words,2,0,1,2,second));
    ee_precise_reset_cache();
    assert(slot->resident_fn&&slot->resident_tested&&slot->fn==first);
    precise_active=0;
    assert(ee_precise_install_slot(slot,0x200100,words,2,0,1,2,second));
    assert(!slot->resident_fn&&!slot->resident_tested&&slot->fn==second);
    slot->resident_fn=(ee_precise_fn)malloc(32);assert(slot->resident_fn);
    slot->resident_tested=1;
    precise_resident_chain_fn=(ee_cached_chain_fn)malloc(32);assert(precise_resident_chain_fn);
    precise_resident_runs=precise_resident_retired=precise_resident_selected=3;
    precise_resident_map_ready=1;precise_resident_map.count=12;
    ee_precise_reset_cache();
    assert(!slot->fn&&!slot->resident_fn&&!slot->resident_tested&&!precise_resident_chain_fn);
    assert(!precise_resident_map_ready&&!precise_resident_map.count);
    assert(ee_jit_get_chain_residency_stat(0)==1);
    for(unsigned i=1;i<5;i++)assert(!ee_jit_get_chain_residency_stat(i));
    puts("PASS native-ABI cache companion ownership, active pin, replacement and reset");
    return 0;
}
