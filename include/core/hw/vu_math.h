#ifndef PCSX2WII_VU_MATH_H
#define PCSX2WII_VU_MATH_H
#include <stdint.h>
/* Primary PCSX2 VUops.cpp fp_min/fp_max: signed integer ordering,
 * reversed when both operands have their sign bit set. No host FP NaN
 * canonicalization or signed-zero tie behavior participates. */
static inline uint32_t vu_minmax_bits(uint32_t a,uint32_t b,int minimum) {
    int both_negative=(a&b&0x80000000u)!=0;
    int choose_a=((int32_t)a<(int32_t)b);
    if(both_negative)choose_a=!choose_a;
    if(!minimum)choose_a=!choose_a;
    return choose_a?a:b;
}
#endif
