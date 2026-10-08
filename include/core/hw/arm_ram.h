#ifndef GEKKO2_ARM_RAM_H
#define GEKKO2_ARM_RAM_H
#include <stdint.h>
/* Experimental synchronous MLOAD jobs over caller-proven ordinary RAM.
 * No guest addresses, code or MMIO. 1 = fully verified bytes; 0 = caller
 * must execute its normal CPU implementation, including after partial failure.
 * Aligned nonoverlapping spans only; minimum 64 KiB, maximum 1 MiB.
 * This is memory assistance, not EE/IOP execution or asynchronous parallelism. */
void arm_ram_reset(void);
int arm_ram_copy(uint8_t *destination,const uint8_t *source,unsigned length);
int arm_ram_fill(uint8_t *destination,uint8_t value,unsigned length);
int arm_ram_available(void);
/* probes, submitted chunks, completed jobs, verified bytes, failures, declines */
uint64_t arm_ram_stat(unsigned n);
#endif
