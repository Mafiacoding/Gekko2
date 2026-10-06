#include "core/hw/frontend_runtime.h"
uint32_t frontend_next_budget(uint32_t previous,uint32_t elapsed_ms,uint32_t target_ms)
{
    if(!elapsed_ms)elapsed_ms=1;
    uint64_t next=(uint64_t)previous*target_ms/elapsed_ms;
    /* Bound growth to avoid a fast boot phase creating a huge next chunk.
     * Shrink immediately when the current phase gets expensive. */
    if(next>(uint64_t)previous*2)next=(uint64_t)previous*2;
    if(next<32)next=32;
    if(next>50000)next=50000;
    return (uint32_t)next;
}
__attribute__((noinline)) uint32_t frontend_rate_milli(uint64_t count,uint64_t milliseconds)
{
    if(!milliseconds)return 0;
    /* Saturate rather than overflow if an invalid/corrupt counter is supplied. */
    if(count>UINT64_MAX/1000000u)return UINT32_MAX;
    uint64_t rate=count*1000000u/milliseconds;
    return rate>UINT32_MAX?UINT32_MAX:(uint32_t)rate;
}
int frontend_present_due(uint64_t now_ms,uint64_t last_ms,uint64_t events,uint64_t last_events)
{
    return events!=last_events || now_ms<last_ms || now_ms-last_ms>=500;
}

int frontend_diagnostic_due(uint64_t now_ms,uint64_t last_ms,uint32_t interval_ms)
{
    /* A zero interval is treated as the conservative one-second diagnostic
     * cadence rather than an unlimited redraw rate. Clock rollback forces one
     * update so callers can recover their baseline without getting stuck. */
    if(!interval_ms)interval_ms=1000;
    return now_ms<last_ms || now_ms-last_ms>=(uint64_t)interval_ms;
}

void frontend_fps_push(frontend_fps_window *w,uint64_t ms,uint64_t events,uint64_t presents)
{
    if(!w||!ms)return;
    w->milliseconds[w->next]=ms;w->events[w->next]=events;w->presents[w->next]=presents;
    w->next=(w->next+1)%FRONTEND_FPS_SAMPLES;
    if(w->count<FRONTEND_FPS_SAMPLES)w->count++;
}
static uint32_t window_rate(const frontend_fps_window *w,int output)
{
    if(!w)return 0;
    uint64_t ms=0,count=0;
    for(unsigned i=0;i<w->count;i++){
        ms+=w->milliseconds[i];count+=output?w->presents[i]:w->events[i];
    }
    return frontend_rate_milli(count,ms);
}
uint32_t frontend_fps_guest(const frontend_fps_window *w){return window_rate(w,0);}
uint32_t frontend_fps_output(const frontend_fps_window *w){return window_rate(w,1);}

/* Detection is independent of HUD and GX preferences. GPU drawing is gated
 * behind the first confirmed software image, even when explicitly requested. */
int frontend_probe_first_image(int due,int active,int first)
{return due&&active&&!first;}
int frontend_allow_gx_primitives(int requested,int first)
{return requested&&first;}

int frontend_allow_gx_output(int requested,int first,unsigned psm)
{return requested && first && (psm==0u || psm==1u);}
