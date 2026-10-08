#ifndef GEKKO2_ARM_WORKER_H
#define GEKKO2_ARM_WORKER_H
#include <stdint.h>
#define ARM_WORKER_MAGIC 0x474b4131u
#define ARM_WORKER_VERSION 1u
#define ARM_WORKER_MAX_BLOCKS 8u
/* All 32-bit wire fields are big endian; input/output are guest bytes.
 * /dev/gekko2: ioctl 0 = 32-byte capability query, ioctlv 1 = CSC.
 * job header: magic/version/epoch/sequence/blocks/options/TH0/TH1.
 * No guest addresses, pointers, JIT code or mutable guest RAM cross IPC. */
int gekko2_arm_dispatch(unsigned request,const uint8_t *header,unsigned header_size,
 const uint8_t *input,unsigned input_size,uint8_t *output,unsigned output_size);
void arm_worker_reset(void);
int arm_worker_available(void);
int arm_worker_completion_ready(void);
int arm_worker_submit_csc(const uint8_t input[384],uint32_t command,uint16_t th0,uint16_t th1);
/* 0 pending, 1 valid completed result copied, -1 unavailable/failed/stale. */
int arm_worker_take(uint8_t *out,unsigned size);
uint64_t arm_worker_stat(unsigned n);
#endif
