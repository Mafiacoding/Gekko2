/* Synthetic control-path fixture; no private BIOS/disc data needed. */
#include "core/recompiler/optimization.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    const uint32_t required = GEKKO2_OPT_BIT(GEKKO2_OPT_EE_JIT)
        | GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS)
        | GEKKO2_OPT_BIT(GEKKO2_OPT_NATIVE_LINKS)
        | GEKKO2_OPT_BIT(GEKKO2_OPT_RESIDENCY);
    assert(!gekko2_chain128_requested());
    assert(!gekko2_chain128_active());
    gekko2_optimization_mask = required;
    gekko2_chain128_request(1);
    /* Staging must never mutate a live translated block. */
    assert(gekko2_chain128_requested());
    assert(!gekko2_chain128_active());
    gekko2_chain128_apply_cold_boot();
#ifdef GEKKO2_EE_CHAIN_RESIDENCY
    assert(gekko2_chain128_active());
#else
    assert(!gekko2_chain128_active());
#endif
    gekko2_chain128_request(0);
#ifdef GEKKO2_EE_CHAIN_RESIDENCY
    assert(gekko2_chain128_active()); /* Still active until cold boot. */
#endif
    gekko2_chain128_apply_cold_boot();
    assert(!gekko2_chain128_active());
    gekko2_chain128_request(1);
    gekko2_optimization_mask=required & ~GEKKO2_OPT_BIT(GEKKO2_OPT_NATIVE_LINKS);
    gekko2_chain128_apply_cold_boot();
    assert(!gekko2_chain128_active()); /* Missing required predecessor. */
    gekko2_optimization_mask=required;
    gekko2_chain128_apply_cold_boot();
#ifdef GEKKO2_EE_CHAIN_RESIDENCY
    assert(gekko2_chain128_active());
#else
    assert(!gekko2_chain128_active());
#endif
    puts("PASS Chain128 runtime OFF/ON, cold-boot staging and prerequisite guards");
    return 0;
}
