#ifndef PCSX2WII_VU_JIT_H
#define PCSX2WII_VU_JIT_H
#include <stdint.h>
/* Native arithmetic on the real arrays; pair flags,
 * GIF/VIF effects and unsupported operations stay in vu_micro_step. */
int vu_jit_try_upper(uint32_t vf[32][4],uint32_t *vi,uint32_t acc[4],uint32_t word);
int vu_jit_try_lower(uint32_t vf[32][4],uint32_t *vi,uint8_t *mem,uint32_t mem_mask,uint32_t word,
                     uint32_t pc,uint32_t *branch_delay,uint32_t *branch_target);
int vu_jit_try_pair(uint32_t vf[32][4],uint32_t *vi,uint32_t *acc,uint8_t *mem,
                    uint32_t mask,uint32_t pc,uint32_t *delay,uint32_t *target,
                    uint32_t upper,uint32_t lower);
unsigned vu_jit_try_block(uint32_t vf[32][4],uint32_t *vi,uint32_t *acc,uint8_t *mem,
                    uint32_t mask,uint8_t *micro,uint32_t micro_mask,uint32_t *pc,
                    uint32_t *delay,uint32_t *target,uint32_t *ebit,
                    uint64_t *retired,unsigned budget);
uint64_t vu_jit_get_block_count(void);
uint64_t vu_jit_get_pair_count(void);
uint64_t vu_jit_get_upper_count(void);
uint64_t vu_jit_get_lower_count(void);
uint64_t vu_jit_get_rejected_hit_count(void);
uint32_t vu_jit_get_cache_size(void);
void vu_jit_reset_for_test(void);
#endif
