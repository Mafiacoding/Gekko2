#ifndef BENCH_VIDEO_H
#define BENCH_VIDEO_H
#include <stdint.h>
void bench_video_init(void);
int bench_fat_init(void);
void bench_wait(void);
uint64_t bench_now_ms(void);
#endif
