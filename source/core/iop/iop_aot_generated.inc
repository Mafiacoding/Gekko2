/* Empty by default; generate from private observed IOP entries. */
uint64_t iop_aot_executed_count(void){return 0;}
static unsigned iop_aot_try_execute(iop_state_t *st,unsigned budget){(void)st;(void)budget;return 0;}
