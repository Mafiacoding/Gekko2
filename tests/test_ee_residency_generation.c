#include <assert.h>
#include <stdio.h>
#include "core/ee/ee_hle_thread.c"
int main(void)
{
 ee_state_t *st=ee_core_get_state();memset(st,0,sizeof(*st));ee_hle_thread_init();
 g.threads[0].in_use=1;g.threads[0].gpr[1].ud0=0x123456789abcdef0ull;
 g.threads[0].gpr[1].ud1=0xfedcba9876543210ull;
 st->gpr_generation=0xffffffffu;load_context(st,1);
 assert(st->gpr_generation==0&&st->gpr[1].ud0==0x123456789abcdef0ull&&st->gpr[1].ud1==0xfedcba9876543210ull);
 load_context(st,1);assert(st->gpr_generation==1);
 load_context(st,0);assert(st->gpr_generation==1);
 puts("PASS real EE thread context replacement invalidates resident copies, including generation wrap");return 0;
}
