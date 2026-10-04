#include "core/hw/frontend_runtime.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void)
{
 assert(frontend_rate_milli(3,5000)==600); /* 0.60 guest FPS */
 assert(frontend_rate_milli(1,12500)==80); /* 0.08 FPS */
 assert(frontend_rate_milli(60,1000)==60000);
 assert(frontend_rate_milli(0,0)==0);
 assert(frontend_rate_milli(UINT64_MAX,1)==UINT32_MAX);
 assert(frontend_next_budget(50000,5000,50)==500);
 assert(frontend_next_budget(512,0,50)==1024);
 assert(frontend_next_budget(32,1000000,20)==32);
 assert(frontend_next_budget(50000,1,50)==50000);
 assert(!frontend_present_due(499,0,10,10));
 assert(frontend_present_due(500,0,10,10));
 assert(frontend_present_due(1,0,11,10));
 assert(frontend_present_due(0,1,10,10));
 frontend_fps_window w={0};
 assert(frontend_fps_guest(&w)==0);
 for(unsigned i=0;i<12;i++)frontend_fps_push(&w,5000,i%2==0?1:0,10);
 assert(frontend_fps_guest(&w)==100); /* six events / 60 seconds */
 assert(frontend_fps_output(&w)==2000);
 frontend_fps_push(&w,5000,0,10); /* oldest event falls out */
 assert(frontend_fps_guest(&w)==83);
 frontend_fps_push(&w,0,999,999); /* missing interval ignored */
 assert(frontend_fps_guest(&w)==83);
 puts("PASS measured guest FPS, adaptive budgets and display cadence");
 return 0;
}
