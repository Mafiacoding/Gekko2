#ifndef WII_LIMITS_KERNELS_H
#define WII_LIMITS_KERNELS_H
#include <stdint.h>
#include <stddef.h>
uint32_t limits_integer(uint32_t x, uint32_t count);
float limits_float(float x, uint32_t count);
uint32_t limits_chase(const volatile uint32_t *p, uint32_t index, uint32_t count);
void limits_ring(uint32_t *p, size_t bytes);
uint32_t limits_pattern(size_t index);
void limits_fill(uint32_t *p, size_t bytes);
int limits_verify(const uint32_t *p, size_t bytes);
#endif
