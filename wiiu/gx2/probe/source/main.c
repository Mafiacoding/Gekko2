/* GPL-3.0-or-later. GX2 probe for the Wii U test port. Two tests, chosen by what is on the SD card:
 *  - <sd>/wiiu/gekko2/present.gsh exists  -> test A: GPU presenter (texture + quad, shaders from present.gsh)
 *  - otherwise                            -> test B: shader-free hardware scaling: a 640x480 TV scan buffer
 *                                            (GX2_TV_RENDER_MODE_STANDARD_480P) that the console scales to the
 *                                            output resolution, written by the CPU.
 * Every step is logged to <sd>/wiiu/gekko2/gx2probe.log (fsync'd) with "about to ..." before risky calls.
 * Nothing here is verified by the author on Cemu or a Wii U; the log tells us what happens. */
#include "gx2_present.h"
#include <coreinit/cache.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <gx2/display.h>
#include <gx2/event.h>
#include <gx2/mem.h>
#include <gx2/state.h>
#include <gx2/swap.h>
#include <whb/proc.h>
#include <whb/sdcard.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static FILE *logf;
static void say(const char *fmt, ...)
{
    char b[200];
    va_list a;
    va_start(a, fmt);
    vsnprintf(b, sizeof b, fmt, a);
    va_end(a);
    if (logf) {
        fprintf(logf, "%s\n", b);
        fflush(logf);
        fsync(fileno(logf));
    }
}

/* Test pattern: colour bars on top, a grey ramp below, a white frame and a box that moves with the frame count. */
static void pattern(uint32_t *fb, unsigned w, unsigned h, unsigned frame)
{
    static const uint32_t bars[8] = { 0xffffff, 0xffff00, 0x00ffff, 0x00ff00, 0xff00ff, 0xff0000, 0x0000ff, 0x000000 };
    for (unsigned y = 0; y < h; y++)
        for (unsigned x = 0; x < w; x++) {
            uint32_t c;
            if (y < h / 2)
                c = bars[x * 8 / w];
            else {
                const unsigned g = x * 255 / w;
                c = (g << 16) | (g << 8) | g;
            }
            if (x < 4 || y < 4 || x >= w - 4 || y >= h - 4)
                c = 0xffffff;
            fb[(size_t)y * w + x] = c;
        }
    const unsigned bx = (frame * 4) % (w - 64), by = h / 2 - 32 + (unsigned)(frame % 32);
    for (unsigned y = by; y < by + 48 && y < h; y++)
        for (unsigned x = bx; x < bx + 48 && x < w; x++)
            fb[(size_t)y * w + x] = 0xff8000;
}

static void *read_file(const char *path, uint32_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *buf = n > 0 ? MEMAllocFromDefaultHeapEx((uint32_t)n, 64) : NULL;
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
        MEMFreeToDefaultHeap(buf);
        buf = NULL;
    }
    fclose(f);
    *size = buf ? (uint32_t)n : 0;
    return buf;
}

static void test_a(void *gsh, uint32_t size)
{
    static uint32_t fb[640 * 480];
    say("test A: about to gx2_present_init (gsh %u bytes)", (unsigned)size);
    if (!gx2_present_init(gsh, size, 640, 480)) {
        say("test A: gx2_present_init FAILED (shader file rejected, or WHBGfxInit failed)");
        return;
    }
    say("test A: init ok; drawing frames (nearest for 5 s, then bilinear)");
    unsigned frame = 0;
    const uint64_t start = OSTicksToMilliseconds(OSGetSystemTime());
    uint64_t last = start;
    unsigned shown = 0;
    while (WHBProcIsRunning()) {
        pattern(fb, 640, 480, frame++);
        const uint64_t now = OSTicksToMilliseconds(OSGetSystemTime());
        gx2_present_frame(fb, 640, 480, now - start > 5000);
        shown++;
        if (now - last >= 1000) {
            say("test A: %u frames in the last second", shown);
            shown = 0;
            last = now;
        }
    }
    gx2_present_shutdown();
}

static void test_b(void)
{
    uint32_t attribs[] = { GX2_INIT_END };
    say("test B: about to GX2Init");
    GX2Init(attribs);
    uint32_t size = 0, unk = 0;
    GX2CalcTVSize(GX2_TV_RENDER_MODE_STANDARD_480P, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8, GX2_BUFFERING_MODE_SINGLE, &size, &unk);
    say("test B: 480p single buffer needs %u bytes (unk=%u)", (unsigned)size, (unsigned)unk);
    void *buf = MEMAllocFromDefaultHeapEx(size, GX2_SCAN_BUFFER_ALIGNMENT);
    if (!buf) {
        say("test B: no memory for the scan buffer");
        return;
    }
    memset(buf, 0, size);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU, buf, size);
    say("test B: about to GX2SetTVBuffer 640x480");
    GX2SetTVBuffer(buf, size, GX2_TV_RENDER_MODE_STANDARD_480P, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8, GX2_BUFFERING_MODE_SINGLE);
    GX2SetTVEnable(TRUE);
    say("test B: scan buffer set; writing the pattern (RGBA, pitch 640 assumed)");
    static uint32_t fb[640 * 480];
    unsigned frame = 0;
    while (WHBProcIsRunning()) {
        pattern(fb, 640, 480, frame++);
        uint32_t *dst = buf;
        for (unsigned i = 0; i < 640u * 480u && (i + 1) * 4u <= size; i++)
            dst[i] = (fb[i] << 8) | 0xffu;
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU, buf, size);
        GX2WaitForVsync();
        if (frame % 120 == 0)
            say("test B: frame %u", frame);
    }
    GX2SetTVEnable(FALSE);
    GX2Shutdown();
    MEMFreeToDefaultHeap(buf);
}

int main(void)
{
    WHBProcInit();
    char path[256] = "";
    if (WHBMountSdCard()) {
        snprintf(path, sizeof path, "%s/wiiu/gekko2/gx2probe.log", WHBGetSdCardMountPath());
        logf = fopen(path, "w");
    }
    say("Gekko2 GX2 probe (log: sd:/wiiu/gekko2/gx2probe.log, %s)", logf ? "open" : "not available");
    uint32_t size = 0;
    void *gsh = NULL;
    if (WHBGetSdCardMountPath()) {
        snprintf(path, sizeof path, "%s/wiiu/gekko2/present.gsh", WHBGetSdCardMountPath());
        gsh = read_file(path, &size);
    }
    if (gsh) {
        say("present.gsh found (%u bytes): test A", (unsigned)size);
        test_a(gsh, size);
    } else {
        say("no present.gsh on the SD card: test B (shader-free hardware scaling)");
        test_b();
    }
    say("done");
    if (logf)
        fclose(logf);
    WHBUnmountSdCard();
    WHBProcShutdown();
    return 0;
}
