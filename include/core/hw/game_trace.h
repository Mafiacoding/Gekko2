#ifndef GEKKO2_GAME_TRACE_H
#define GEKKO2_GAME_TRACE_H
#include <stdint.h>
#include <stdio.h>
enum {GT_TAG=1,GT_KICK,GT_SIGNAL,GT_SIGNAL_WAIT,GT_SIGNAL_RESUME,GT_CSR_ACK,GT_IMR,GT_VIF_WAIT,GT_VIF_RESUME,GT_GIF_WAIT,GT_GIF_RESUME,GT_VU_UNKNOWN,GT_INTC_ACK,GT_INTC_MASK,GT_TAG_PATH1=16};
#ifdef GEKKO2_GAME_TRACE
void game_trace_reset(void);
void game_trace_record(unsigned kind,uint32_t a,uint32_t b,uint32_t c,uint32_t d);
void game_trace_dump(FILE *out,uint64_t *cursor);
#else
static inline void game_trace_reset(void){}
static inline void game_trace_record(unsigned kind,uint32_t a,uint32_t b,uint32_t c,uint32_t d){(void)kind;(void)a;(void)b;(void)c;(void)d;}
static inline void game_trace_dump(FILE *out,uint64_t *cursor){(void)out;(void)cursor;}
#endif
#endif
