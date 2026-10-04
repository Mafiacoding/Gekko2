#ifndef FRONTEND_RUNTIME_H
#define FRONTEND_RUNTIME_H
#include <stdint.h>
/* Host scheduling/display metrics. Never alter guest clock or instruction order. */
int frontend_probe_first_image(int present_due,int display_active,int first_image);
int frontend_allow_gx_primitives(int requested,int first_image);
/* CT24 shares CT32 RGB addressing; presentation ignores the alpha byte. */
int frontend_allow_gx_output(int requested,int first_image,unsigned psm);
uint32_t frontend_next_budget(uint32_t previous,uint32_t elapsed_ms,uint32_t target_ms);
uint32_t frontend_rate_milli(uint64_t count,uint64_t milliseconds);
int frontend_present_due(uint64_t now_ms,uint64_t last_ms,uint64_t events,uint64_t last_events);

#define FRONTEND_FPS_SAMPLES 12
/* Twelve five-second samples: up to one minute of measured wall time. */
typedef struct {
    uint64_t milliseconds[FRONTEND_FPS_SAMPLES];
    uint64_t events[FRONTEND_FPS_SAMPLES];
    uint64_t presents[FRONTEND_FPS_SAMPLES];
    unsigned next,count;
} frontend_fps_window;
void frontend_fps_push(frontend_fps_window *window,uint64_t ms,uint64_t events,uint64_t presents);
uint32_t frontend_fps_guest(const frontend_fps_window *window);
uint32_t frontend_fps_output(const frontend_fps_window *window);
#endif
