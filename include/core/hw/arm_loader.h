/* GPL-3.0+. Bounded ELF loader for an explicitly selected MLOAD RAM service. */
#ifndef GEKKO2_ARM_LOADER_H
#define GEKKO2_ARM_LOADER_H
#include <stdint.h>
#define ARM_LOADER_MAX_SEGMENTS 8
typedef struct { uint32_t address,offset,file_size,memory_size,flags; } arm_load_segment_t;
typedef struct {
 uint32_t entry,stack,stack_size,priority,count;
 arm_load_segment_t segment[ARM_LOADER_MAX_SEGMENTS];
} arm_load_plan_t;
int arm_loader_validate(const uint8_t *elf,unsigned size,uint32_t base,uint32_t capacity,arm_load_plan_t *plan);
/* 1 thread started; negative status preserves CPU fallback. No IOS reload. */
int arm_loader_start(void);
int arm_loader_status(void);
/* file bytes, IOS base, IOS capacity, ELF entry, file CRC32, validation stage. */
uint32_t arm_loader_stat(unsigned n);
#endif
