#ifndef GEKKO2_EE_AOT_H
#define GEKKO2_EE_AOT_H
#include "core/ee/ee_core.h"
/* Opt-in, build-time generated ALU blocks. Zero means untouched fallback. */
unsigned ee_aot_try_execute_fetched(ee_state_t *st, unsigned budget, uint32_t first_word);
unsigned ee_aot_block_count(void);
uint64_t ee_aot_retired_count(void);
/* Exact-word native handlers specialized from shared COP/MMI semantics. */
int ee_aot_execute_complex(ee_state_t *st,uint32_t word);
int ee_aot_complex_available(uint32_t word);
#endif
