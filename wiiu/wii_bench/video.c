/* GPL-3.0-or-later. libogc side of the bench; the only file compiled with GEKKO defined. */
#define GEKKO 1
#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include <fat.h>
#include <stdint.h>
#include "bench_video.h"
static void *xfb;
static GXRModeObj *rmode;
void bench_video_init(void)
{
    VIDEO_Init();
    rmode = VIDEO_GetPreferredMode(NULL);
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(0);
    VIDEO_Flush();
    VIDEO_WaitVSync();
}
int bench_fat_init(void) { return fatInitDefault(); }
void bench_wait(void) { VIDEO_WaitVSync(); }
uint64_t bench_now_ms(void) { return (uint64_t)ticks_to_millisecs(gettime()); }
