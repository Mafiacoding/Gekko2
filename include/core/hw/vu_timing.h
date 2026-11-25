#ifndef GEKKO2_VU_TIMING_H
#define GEKKO2_VU_TIMING_H
#include <stdint.h>
/* Serialized per-unit FMAC state. Four issue slots, four-cycle latency.
 * Arithmetic, FSSET and CLIP/FCSET share the queue; FDIV owns only STATUS I/D.
 * set: bit 0 FSSET, bit 1 CLIP write, bit 2 no MAC/STATUS write.
 * clip occupies previous trailing padding: slot size/offsets stay unchanged. */
typedef struct {
    struct { uint64_t ready; uint32_t mac,status; uint8_t set; uint32_t clip; } flags[4];
    uint8_t head,count,m_yield;
    /* VI branch bypass: these fields occupy the old pre-scoreboard padding.
     * marker distinguishes legacy checkpoints with no VI history. */
    uint8_t vi_age;
    uint16_t vi_old;
    uint8_t vi_reg,vi_marker;
    uint64_t vf_ready[32][4];
} vu_timing_t;
#endif
