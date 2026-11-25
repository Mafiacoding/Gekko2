#include "kernels.h"
/* Dependent recurrences, not claimed IPC/FLOPS or PS2 instructions. */
__attribute__((noinline)) uint32_t limits_integer(uint32_t x, uint32_t count)
{
    for (uint32_t i=0;i<count;i++) {
        x=x*1664525u+1013904223u;
        x^=x>>13; x=(x<<7)|(x>>25); x+=0x9e3779b9u;
    }
    return x;
}
__attribute__((noinline)) float limits_float(float x, uint32_t count)
{
    for (uint32_t i=0;i<count;i++) { x=x*0.99991f+0.00013f; x=x*0.99993f+0.00011f; }
    return x;
}
__attribute__((noinline)) uint32_t limits_chase(const volatile uint32_t *p, uint32_t index, uint32_t count)
{
    for (uint32_t i=0;i<count;i++) index=p[index];
    return index;
}
void limits_ring(uint32_t *p, size_t bytes)
{
    /* One dependent load per 32B line. Full-period LCG modulo power of two;
       count must be a power of two, proven by the caller and host test. */
    uint32_t lines=(uint32_t)(bytes/32);
    for (uint32_t i=0;i<lines;i++) p[i*8]=((i*1664525u+1013904223u)&(lines-1))*8;
}
uint32_t limits_pattern(size_t index) { return (uint32_t)index*0x9e3779b9u^0xa55a3cc3u; }
void limits_fill(uint32_t *p, size_t bytes)
{ for (size_t i=0;i<bytes/4;i++) p[i]=limits_pattern(i); }
int limits_verify(const uint32_t *p, size_t bytes)
{ for (size_t i=0;i<bytes/4;i++) if(p[i]!=limits_pattern(i))return 0; return 1; }
