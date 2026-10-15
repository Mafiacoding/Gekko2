#ifndef GEKKO2_VU_TIMING_H
#define GEKKO2_VU_TIMING_H
#include <stdint.h>
/* Serialized per-unit FMAC state. Four issue slots, four-cycle latency.
 * Arithmetic and FSSET share the queue; FDIV owns only STATUS I/D. */
typedef struct {
    struct { uint64_t ready; uint32_t mac,status; uint8_t set; } flags[4];
    uint8_t head,count,m_yield;
    uint64_t vf_ready[32][4];
} vu_timing_t;
#endif
